#include "pch.h"
#include "StarPipeline.h"

#include "CompiledShader/StarCrossPS.h"
#include "CompiledShader/StarPS.h"
#include "CompiledShader/StarSpritePS.h"
#include "CompiledShader/StarVS.h"

namespace
{
// The root signature's parameters: the frame's constants at b0, as root constants, and for a sprite its texture at t0.
constexpr UINT FRAME_PARAMETER = 0;
constexpr UINT SPRITE_PARAMETER = 1;
constexpr UINT FRAME_CONSTANT_COUNT = sizeof(Neuron::StarPipeline::FrameConstants) / sizeof(UINT);
// Each star is a quad drawn as a strip of two triangles, its corners made by the vertex shader from SV_VertexID.
constexpr UINT VERTICES_PER_STAR = 4;

winrt::com_ptr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* _device, bool _sprite)
{
  std::array<CD3DX12_ROOT_PARAMETER1, 2> parameters{};
  parameters[FRAME_PARAMETER].InitAsConstants(FRAME_CONSTANT_COUNT, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  const CD3DX12_DESCRIPTOR_RANGE1 spriteRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
  parameters[SPRITE_PARAMETER].InitAsDescriptorTable(1, &spriteRange, D3D12_SHADER_VISIBILITY_PIXEL);
  // Trilinear, so that a sprite drawn smaller than its texture reads its mip levels and its spikes do not shimmer.
  const CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC description;
  description.Init_1_1(_sprite ? 2u : 1u, parameters.data(), _sprite ? 1u : 0u, _sprite ? &sampler : nullptr,
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

Neuron::StarPipeline::StarPipeline(Renderer& _renderer, std::span<const Star> _stars, Shape _shape, const TextureData* _sprite)
  : m_starCount(static_cast<UINT>(_stars.size()))
{
  if (_shape != Shape::Sprite)
    _sprite = nullptr;
  else if (_sprite == nullptr)
    throw winrt::hresult_error(E_INVALIDARG, L"A sky of sprite stars needs its sprite.");

  ID3D12Device* device = _renderer.Device();
  m_rootSignature = CreateRootSignature(device, _sprite != nullptr);

  // Matches Star, one per instance.
  const std::array<D3D12_INPUT_ELEMENT_DESC, 3> inputLayout{{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Star, direction), D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    {"TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(Star, radiusPixels), D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
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

  CD3DX12_SHADER_BYTECODE pixelShader(g_StarPS, sizeof(g_StarPS));
  if (_shape == Shape::Sprite)
    pixelShader = CD3DX12_SHADER_BYTECODE(g_StarSpritePS, sizeof(g_StarSpritePS));
  else if (_shape == Shape::Cross)
    pixelShader = CD3DX12_SHADER_BYTECODE(g_StarCrossPS, sizeof(g_StarCrossPS));

  // Every member with an enum that has no zero value is set here, so none is ever left at an invalid zero.
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC description{
    .pRootSignature = m_rootSignature.get(),
    .VS = CD3DX12_SHADER_BYTECODE(g_StarVS, sizeof(g_StarVS)),
    .PS = pixelShader,
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
  if (_sprite == nullptr)
    return;

  std::vector<std::span<const std::byte>> levels;
  levels.reserve(_sprite->levels.size());
  for (const ByteBuffer& level : _sprite->levels)
    levels.push_back(std::as_bytes(std::span(level)));
  m_sprite = _renderer.CreateStaticTexture(_sprite->width, _sprite->height, _sprite->format, levels);
  const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .NumDescriptors = 1, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, .NodeMask = 0};
  winrt::check_hresult(device->CreateDescriptorHeap(&heapDescription, IID_GRAPHICS_PPV_ARGS(m_descriptorHeap)));
  const D3D12_SHADER_RESOURCE_VIEW_DESC view{
    .Format = _sprite->format,
    .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = static_cast<UINT>(levels.size()), .PlaneSlice = 0, .ResourceMinLODClamp = 0.0f}};
  device->CreateShaderResourceView(m_sprite.get(), &view, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
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
  if (m_descriptorHeap)
  {
    ID3D12DescriptorHeap* heaps[] = {m_descriptorHeap.get()};
    _commandList->SetDescriptorHeaps(1, heaps);
    _commandList->SetGraphicsRootDescriptorTable(SPRITE_PARAMETER, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
  }
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  _commandList->IASetVertexBuffers(0, 1, &instanceView);
  _commandList->DrawInstanced(VERTICES_PER_STAR, m_starCount, 0, 0);
}
