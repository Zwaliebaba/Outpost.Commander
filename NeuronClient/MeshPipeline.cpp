#include "pch.h"
#include "MeshPipeline.h"

#include "CompiledShader/MeshLineVS.h"
#include "CompiledShader/MeshPS.h"
#include "CompiledShader/MeshVS.h"

#include <cstring>

namespace
{
// The root signature's parameters, in order: the frame's constant buffer at b0, the object's constants at b1.
constexpr UINT FRAME_PARAMETER = 0;
constexpr UINT OBJECT_PARAMETER = 1;

// cbuffer Object in the shaders: the world matrix, the color, and how far a line is pulled toward the eye. Whole
// float4s, so that the root constants cover the cbuffer as the shader compiler sizes it.
struct ObjectConstants
{
  DirectX::XMFLOAT4X4 world;
  DirectX::XMFLOAT4 color;
  float liftShare;
  float unused0;
  float unused1;
  float unused2;
};

constexpr UINT OBJECT_CONSTANT_COUNT = sizeof(ObjectConstants) / sizeof(UINT);

// DrawTriangles' vertices are in the world already.
constexpr DirectX::XMFLOAT4X4 IDENTITY{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

winrt::com_ptr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* _device)
{
  std::array<CD3DX12_ROOT_PARAMETER1, 2> parameters{};
  parameters[FRAME_PARAMETER].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
  parameters[OBJECT_PARAMETER].InitAsConstants(OBJECT_CONSTANT_COUNT, 1);

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC description;
  description.Init_1_1(static_cast<UINT>(parameters.size()), parameters.data(), 0, nullptr,
                       D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  // Version 1.1 where the device has it, 1.0 otherwise; d3dx12 converts the description.
  D3D12_FEATURE_DATA_ROOT_SIGNATURE feature{.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1};
  if (FAILED(_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature, sizeof(feature))))
    feature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

  winrt::com_ptr<ID3DBlob> blob;
  winrt::com_ptr<ID3DBlob> error;
  const HRESULT result = D3DX12SerializeVersionedRootSignature(&description, feature.HighestVersion, blob.put(), error.put());
  if (FAILED(result))
  {
    const std::string_view message = error ? std::string_view(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize())
                                           : std::string_view("no details");
    throw winrt::hresult_error(result, winrt::to_hstring(std::format("The mesh root signature is invalid: {}", message)));
  }

  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(
    _device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(rootSignature)));
  return rootSignature;
}
} // namespace

Neuron::MeshPipeline::MeshPipeline(Renderer& _renderer)
{
  ID3D12Device* device = _renderer.Device();
  m_rootSignature = CreateRootSignature(device);

  // Matches MeshVertex.
  const std::array<D3D12_INPUT_ELEMENT_DESC, 2> inputLayout{{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(MeshVertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(MeshVertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  }};

  // The defaults cull back faces, which are the counterclockwise ones, and test depth with less-than (ADR-011).
  // Every member with an enum that has no zero value is set here, so none is ever left at an invalid zero.
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC description{
    .pRootSignature = m_rootSignature.get(),
    .VS = CD3DX12_SHADER_BYTECODE(g_MeshVS, sizeof(g_MeshVS)),
    .PS = CD3DX12_SHADER_BYTECODE(g_MeshPS, sizeof(g_MeshPS)),
    .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
    .SampleMask = UINT_MAX,
    .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
    .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
    .InputLayout = {.pInputElementDescs = inputLayout.data(), .NumElements = static_cast<UINT>(inputLayout.size())},
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = {Renderer::RENDER_TARGET_FORMAT},
    .DSVFormat = Renderer::DEPTH_FORMAT,
    .SampleDesc = {.Count = Renderer::SAMPLE_COUNT, .Quality = 0},
  };
  winrt::check_hresult(device->CreateGraphicsPipelineState(&description, IID_GRAPHICS_PPV_ARGS(m_pipelineState)));

  // Lines: the same pixel shader, so a line is lit as the surface it lies on, rasterized as a line list. They are tested
  // against the depth of what is drawn but write none, so the line drawn last never hides another. Direct3D gives a
  // line no depth bias, so a line that must show over a surface is pulled toward the eye by its own vertex shader
  // (ADR-040). Multisampling on, a line is a quadrilateral a pixel wide that the samples smooth.
  CD3DX12_DEPTH_STENCIL_DESC lineDepth(D3D12_DEFAULT);
  lineDepth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  lineDepth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  CD3DX12_RASTERIZER_DESC lineRasterizer(D3D12_DEFAULT);
  lineRasterizer.MultisampleEnable = TRUE;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC lineDescription = description;
  lineDescription.VS = CD3DX12_SHADER_BYTECODE(g_MeshLineVS, sizeof(g_MeshLineVS));
  lineDescription.RasterizerState = lineRasterizer;
  lineDescription.DepthStencilState = lineDepth;
  lineDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
  winrt::check_hresult(device->CreateGraphicsPipelineState(&lineDescription, IID_GRAPHICS_PPV_ARGS(m_lineState)));

  const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
  const CD3DX12_RESOURCE_DESC bufferDescription = CD3DX12_RESOURCE_DESC::Buffer(UINT64{FRAME_CONSTANTS_BYTES} * Renderer::FRAME_COUNT);
  winrt::check_hresult(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDescription,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                       IID_GRAPHICS_PPV_ARGS(m_frameConstants)));
  const D3D12_RANGE nothingRead{.Begin = 0, .End = 0};
  void* mapped = nullptr;
  winrt::check_hresult(m_frameConstants->Map(0, &nothingRead, &mapped));
  m_mappedFrameConstants = static_cast<std::byte*>(mapped);

  const CD3DX12_RESOURCE_DESC verticesDescription =
    CD3DX12_RESOURCE_DESC::Buffer(UINT64{sizeof(MeshVertex)} * MAX_FRAME_VERTICES * Renderer::FRAME_COUNT);
  winrt::check_hresult(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &verticesDescription,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_GRAPHICS_PPV_ARGS(m_frameVertices)));
  void* mappedVertices = nullptr;
  winrt::check_hresult(m_frameVertices->Map(0, &nothingRead, &mappedVertices));
  m_mappedFrameVertices = static_cast<MeshVertex*>(mappedVertices);
}

void Neuron::MeshPipeline::BeginDrawing(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, const FrameConstants& _constants)
{
  // The renderer waited for this frame index's previous frame before handing out the command list, so the GPU is done
  // with this slot.
  const UINT64 offset = UINT64{FRAME_CONSTANTS_BYTES} * _frameIndex;
  std::memcpy(m_mappedFrameConstants + offset, &_constants, sizeof(FrameConstants));
  m_frameIndex = _frameIndex;
  m_frameVerticesUsed = 0;

  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _commandList->SetGraphicsRootConstantBufferView(FRAME_PARAMETER, m_frameConstants->GetGPUVirtualAddress() + offset);
}

void Neuron::MeshPipeline::Draw(ID3D12GraphicsCommandList* _commandList, const Mesh& _mesh, const DirectX::XMFLOAT4X4& _world,
                                const DirectX::XMFLOAT4& _color) const
{
  DrawObject(_commandList, _mesh, _world, _color, 0.0f);
}

void Neuron::MeshPipeline::DrawObject(ID3D12GraphicsCommandList* _commandList, const Mesh& _mesh, const DirectX::XMFLOAT4X4& _world,
                                      const DirectX::XMFLOAT4& _color, float _liftShare) const
{
  const ObjectConstants constants{
    .world = _world, .color = _color, .liftShare = _liftShare, .unused0 = 0.0f, .unused1 = 0.0f, .unused2 = 0.0f};
  _commandList->SetGraphicsRoot32BitConstants(OBJECT_PARAMETER, OBJECT_CONSTANT_COUNT, &constants, 0);
  _commandList->IASetVertexBuffers(0, 1, &_mesh.VertexBufferView());
  _commandList->IASetIndexBuffer(&_mesh.IndexBufferView());
  _commandList->DrawIndexedInstanced(_mesh.IndexCount(), 1, 0, 0, 0);
}

bool Neuron::MeshPipeline::DrawTriangles(ID3D12GraphicsCommandList* _commandList, std::span<const MeshVertex> _vertices,
                                         const DirectX::XMFLOAT4& _color)
{
  if (_vertices.empty())
    return true;
  if (_vertices.size() > MAX_FRAME_VERTICES - m_frameVerticesUsed)
    return false;
  const auto count = static_cast<UINT>(_vertices.size());
  // The renderer waited for this frame index's previous frame before handing out the command list, so the GPU is done
  // with this slot, as it is with the frame's constants.
  const size_t first = (size_t{m_frameIndex} * MAX_FRAME_VERTICES) + m_frameVerticesUsed;
  std::memcpy(m_mappedFrameVertices + first, _vertices.data(), _vertices.size_bytes());
  m_frameVerticesUsed += count;

  const D3D12_VERTEX_BUFFER_VIEW view{
    .BufferLocation = m_frameVertices->GetGPUVirtualAddress() + (first * sizeof(MeshVertex)),
    .SizeInBytes = static_cast<UINT>(_vertices.size_bytes()),
    .StrideInBytes = sizeof(MeshVertex),
  };
  const ObjectConstants constants{.world = IDENTITY, .color = _color, .liftShare = 0.0f, .unused0 = 0.0f, .unused1 = 0.0f, .unused2 = 0.0f};
  _commandList->SetGraphicsRoot32BitConstants(OBJECT_PARAMETER, OBJECT_CONSTANT_COUNT, &constants, 0);
  _commandList->IASetVertexBuffers(0, 1, &view);
  _commandList->DrawInstanced(count, 1, 0, 0);
  return true;
}

void Neuron::MeshPipeline::DrawLines(ID3D12GraphicsCommandList* _commandList, const Mesh& _lines, const DirectX::XMFLOAT4X4& _world,
                                     const DirectX::XMFLOAT4& _color, float _liftShare) const
{
  // The root signature and the frame's constants stay as BeginDrawing set them; the state and the topology change, and
  // change back.
  const LineDraw draw{.lines = &_lines, .world = _world, .color = _color, .liftShare = _liftShare};
  DrawLines(_commandList, std::span(&draw, 1));
}

void Neuron::MeshPipeline::DrawLines(ID3D12GraphicsCommandList* _commandList, std::span<const LineDraw> _draws) const
{
  if (_draws.empty())
    return;
  _commandList->SetPipelineState(m_lineState.get());
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
  for (const LineDraw& draw : _draws)
    DrawObject(_commandList, *draw.lines, draw.world, draw.color, draw.liftShare);
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
