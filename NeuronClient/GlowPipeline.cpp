#include "pch.h"
#include "GlowPipeline.h"

#include "CompiledShader/GlowPS.h"
#include "CompiledShader/GlowVS.h"

#include <algorithm>
#include <cstring>

namespace
{
// The root signature's one parameter: the frame's constants at b0, as root constants.
constexpr UINT FRAME_PARAMETER = 0;
constexpr UINT FRAME_CONSTANT_COUNT = sizeof(Neuron::GlowPipeline::FrameConstants) / sizeof(UINT);
// Each glow is a quad of two triangles, its corners made by the vertex shader from SV_VertexID.
constexpr UINT VERTICES_PER_GLOW = 6;

winrt::com_ptr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* _device)
{
  std::array<CD3DX12_ROOT_PARAMETER1, 1> parameters{};
  parameters[FRAME_PARAMETER].InitAsConstants(FRAME_CONSTANT_COUNT, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

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
    throw winrt::hresult_error(result, winrt::to_hstring(std::format("The glow root signature is invalid: {}", message)));
  }

  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(
    _device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(rootSignature)));
  return rootSignature;
}
} // namespace

Neuron::GlowPipeline::GlowPipeline(Renderer& _renderer)
{
  ID3D12Device* device = _renderer.Device();
  m_rootSignature = CreateRootSignature(device);

  // Matches Glow, one per instance.
  const std::array<D3D12_INPUT_ELEMENT_DESC, 3> inputLayout{{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Glow, position), D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    {"TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(Glow, radiusMeters), D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Glow, color), D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
  }};

  // Light adds to what is there, so the order the glows are drawn in does not matter. They are tested against the
  // scene's depth, so a hull hides what is behind it, and write none, so a glow never hides another.
  CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);
  blend.RenderTarget[0].BlendEnable = TRUE;
  blend.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
  blend.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
  blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
  blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
  CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
  rasterizer.CullMode = D3D12_CULL_MODE_NONE;
  CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
  depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

  // Every member with an enum that has no zero value is set here, so none is ever left at an invalid zero.
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC description{
    .pRootSignature = m_rootSignature.get(),
    .VS = CD3DX12_SHADER_BYTECODE(g_GlowVS, sizeof(g_GlowVS)),
    .PS = CD3DX12_SHADER_BYTECODE(g_GlowPS, sizeof(g_GlowPS)),
    .BlendState = blend,
    .SampleMask = UINT_MAX,
    .RasterizerState = rasterizer,
    .DepthStencilState = depthStencil,
    .InputLayout = {.pInputElementDescs = inputLayout.data(), .NumElements = static_cast<UINT>(inputLayout.size())},
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = {Renderer::RENDER_TARGET_FORMAT},
    .DSVFormat = Renderer::DEPTH_FORMAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
  };
  winrt::check_hresult(device->CreateGraphicsPipelineState(&description, IID_GRAPHICS_PPV_ARGS(m_pipelineState)));

  const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
  const CD3DX12_RESOURCE_DESC instanceDescription = CD3DX12_RESOURCE_DESC::Buffer(UINT64{sizeof(Glow)} * MAX_GLOWS * Renderer::FRAME_COUNT);
  winrt::check_hresult(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &instanceDescription,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_GRAPHICS_PPV_ARGS(m_instances)));
  const D3D12_RANGE nothingRead{.Begin = 0, .End = 0};
  void* mapped = nullptr;
  winrt::check_hresult(m_instances->Map(0, &nothingRead, &mapped));
  m_mappedInstances = static_cast<Glow*>(mapped);
}

void Neuron::GlowPipeline::Draw(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, const FrameConstants& _constants,
                                std::span<const Glow> _glows)
{
  const auto count = static_cast<UINT>(std::min<size_t>(_glows.size(), MAX_GLOWS));
  if (count == 0)
    return;
  // The renderer waited for this frame index's previous frame before handing out the command list, so the GPU is done
  // with this slot.
  const size_t slot = size_t{_frameIndex} * MAX_GLOWS;
  std::memcpy(m_mappedInstances + slot, _glows.data(), count * sizeof(Glow));

  const D3D12_VERTEX_BUFFER_VIEW instanceView{
    .BufferLocation = m_instances->GetGPUVirtualAddress() + (slot * sizeof(Glow)),
    .SizeInBytes = static_cast<UINT>(count * sizeof(Glow)),
    .StrideInBytes = sizeof(Glow),
  };
  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->SetGraphicsRoot32BitConstants(FRAME_PARAMETER, FRAME_CONSTANT_COUNT, &_constants, 0);
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _commandList->IASetVertexBuffers(0, 1, &instanceView);
  _commandList->DrawInstanced(VERTICES_PER_GLOW, count, 0, 0);
}
