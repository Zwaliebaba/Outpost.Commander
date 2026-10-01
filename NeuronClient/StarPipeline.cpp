#include "pch.h"
#include "StarPipeline.h"

#include "CompiledShader/StarPS.h"
#include "CompiledShader/StarVS.h"

namespace
{
// The root signature's one parameter: the frame's constants at b0, as root constants.
constexpr UINT FRAME_PARAMETER = 0;
constexpr UINT FRAME_CONSTANT_COUNT = sizeof(Neuron::StarPipeline::FrameConstants) / sizeof(UINT);
// Each star is a quad drawn as a strip of two triangles, its corners made by the vertex shader from SV_VertexID.
constexpr UINT VERTICES_PER_STAR = 4;

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
    throw winrt::hresult_error(result, winrt::to_hstring(std::format("The star root signature is invalid: {}", message)));
  }

  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(
    _device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(rootSignature)));
  return rootSignature;
}
} // namespace

Neuron::StarPipeline::StarPipeline(Renderer& _renderer, std::span<const Star> _stars)
  : m_starCount(static_cast<UINT>(_stars.size()))
{
  ID3D12Device* device = _renderer.Device();
  m_rootSignature = CreateRootSignature(device);

  // Matches Star, one per instance.
  const std::array<D3D12_INPUT_ELEMENT_DESC, 3> inputLayout{{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Star, direction), D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    {"TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(Star, spreadPixels), D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Star, color), D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
  }};

  // Starlight adds to the cleared frame, so stars that overlap add up and the order they are drawn in does not matter.
  // The sky is behind everything, so it neither tests nor writes depth: whatever is drawn after it covers it.
  CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);
  blend.RenderTarget[0].BlendEnable = TRUE;
  blend.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
  blend.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
  blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
  blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
  CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
  rasterizer.CullMode = D3D12_CULL_MODE_NONE;
  CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
  depthStencil.DepthEnable = FALSE;
  depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

  // Every member with an enum that has no zero value is set here, so none is ever left at an invalid zero.
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC description{
    .pRootSignature = m_rootSignature.get(),
    .VS = CD3DX12_SHADER_BYTECODE(g_StarVS, sizeof(g_StarVS)),
    .PS = CD3DX12_SHADER_BYTECODE(g_StarPS, sizeof(g_StarPS)),
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

  if (m_starCount > 0)
    m_instances = _renderer.CreateStaticBuffer(std::as_bytes(_stars));
}

void Neuron::StarPipeline::Draw(ID3D12GraphicsCommandList* _commandList, const FrameConstants& _constants) const
{
  if (m_starCount == 0)
    return;
  const D3D12_VERTEX_BUFFER_VIEW instanceView{
    .BufferLocation = m_instances->GetGPUVirtualAddress(),
    .SizeInBytes = static_cast<UINT>(m_starCount * sizeof(Star)),
    .StrideInBytes = sizeof(Star),
  };
  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->SetGraphicsRoot32BitConstants(FRAME_PARAMETER, FRAME_CONSTANT_COUNT, &_constants, 0);
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  _commandList->IASetVertexBuffers(0, 1, &instanceView);
  _commandList->DrawInstanced(VERTICES_PER_STAR, m_starCount, 0, 0);
}
