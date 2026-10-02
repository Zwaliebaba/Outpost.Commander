#include "pch.h"
#include "GroundMaskPipeline.h"

#include "CompiledShader/GroundMaskPS.h"
#include "CompiledShader/GroundMaskVS.h"

#include <algorithm>
#include <cstring>

namespace
{
// The root signature's parameters: the frame's constants at b0, as root constants, and the shades at t0, as a root
// shader resource view straight into the upload buffer.
constexpr UINT FRAME_PARAMETER = 0;
constexpr UINT SHADES_PARAMETER = 1;
constexpr UINT FRAME_CONSTANT_COUNT = sizeof(Neuron::GroundMaskPipeline::FrameConstants) / sizeof(UINT);
// The square is two triangles, its corners made by the vertex shader from SV_VertexID.
constexpr UINT VERTICES_PER_SQUARE = 6;

winrt::com_ptr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* _device)
{
  std::array<CD3DX12_ROOT_PARAMETER1, 2> parameters{};
  parameters[FRAME_PARAMETER].InitAsConstants(FRAME_CONSTANT_COUNT, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
  // The shades are rewritten every frame, in a slot the GPU is done with, before the command list runs.
  parameters[SHADES_PARAMETER].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
                                                        D3D12_SHADER_VISIBILITY_PIXEL);

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC description;
  description.Init_1_1(static_cast<UINT>(parameters.size()), parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

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
    throw winrt::hresult_error(result, winrt::to_hstring(std::format("The ground mask root signature is invalid: {}", message)));
  }

  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(
    _device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(rootSignature)));
  return rootSignature;
}
} // namespace

Neuron::GroundMaskPipeline::GroundMaskPipeline(Renderer& _renderer)
{
  ID3D12Device* device = _renderer.Device();
  m_rootSignature = CreateRootSignature(device);

  // Black, as opaque as the shade: the scene shows through what is left.
  CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);
  blend.RenderTarget[0].BlendEnable = TRUE;
  blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
  blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
  CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
  rasterizer.CullMode = D3D12_CULL_MODE_NONE;
  // Over everything drawn so far, whatever its depth, and writing none.
  CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
  depthStencil.DepthEnable = FALSE;
  depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

  // Every member with an enum that has no zero value is set here, so none is ever left at an invalid zero.
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC description{
    .pRootSignature = m_rootSignature.get(),
    .VS = CD3DX12_SHADER_BYTECODE(g_GroundMaskVS, sizeof(g_GroundMaskVS)),
    .PS = CD3DX12_SHADER_BYTECODE(g_GroundMaskPS, sizeof(g_GroundMaskPS)),
    .BlendState = blend,
    .SampleMask = UINT_MAX,
    .RasterizerState = rasterizer,
    .DepthStencilState = depthStencil,
    .InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0},
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = {Renderer::RENDER_TARGET_FORMAT},
    .DSVFormat = Renderer::DEPTH_FORMAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
  };
  winrt::check_hresult(device->CreateGraphicsPipelineState(&description, IID_GRAPHICS_PPV_ARGS(m_pipelineState)));

  const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
  const CD3DX12_RESOURCE_DESC shadesDescription = CD3DX12_RESOURCE_DESC::Buffer(UINT64{sizeof(float)} * MAX_CELLS * Renderer::FRAME_COUNT);
  winrt::check_hresult(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &shadesDescription,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_GRAPHICS_PPV_ARGS(m_shades)));
  const D3D12_RANGE nothingRead{.Begin = 0, .End = 0};
  void* mapped = nullptr;
  winrt::check_hresult(m_shades->Map(0, &nothingRead, &mapped));
  m_mappedShades = static_cast<float*>(mapped);
}

void Neuron::GroundMaskPipeline::Draw(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, const FrameConstants& _constants,
                                      std::span<const float> _shades)
{
  const size_t cells = size_t{_constants.cellsPerSide} * _constants.cellsPerSide;
  if (cells == 0 || cells > MAX_CELLS || _shades.size() != cells)
    return;
  // The renderer waited for this frame index's previous frame before handing out the command list, so the GPU is done
  // with this slot.
  const size_t slot = size_t{_frameIndex} * MAX_CELLS;
  std::memcpy(m_mappedShades + slot, _shades.data(), cells * sizeof(float));

  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->SetGraphicsRoot32BitConstants(FRAME_PARAMETER, FRAME_CONSTANT_COUNT, &_constants, 0);
  _commandList->SetGraphicsRootShaderResourceView(SHADES_PARAMETER, m_shades->GetGPUVirtualAddress() + (slot * sizeof(float)));
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _commandList->DrawInstanced(VERTICES_PER_SQUARE, 1, 0, 0);
}
