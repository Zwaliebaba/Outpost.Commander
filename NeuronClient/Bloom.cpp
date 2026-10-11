#include "pch.h"
#include "Bloom.h"

#include "CompiledShader/BloomBrightPS.h"
#include "CompiledShader/BloomDownPS.h"
#include "CompiledShader/BloomUpPS.h"
#include "CompiledShader/BloomVS.h"

#include <algorithm>
#include <cmath>

namespace
{
// The root signature's parameters, in order: the level read, as a table of one view at t0, and the pass's constants at b0,
// as root constants (cbuffer Pass in the three pixel shaders).
constexpr UINT SOURCE_PARAMETER = 0;
constexpr UINT PASS_PARAMETER = 1;

// cbuffer Pass in Shader/BloomBrightPS.hlsl, BloomDownPS.hlsl and BloomUpPS.hlsl: the size of a texel of the level read and
// of the level written, as shares of each, the threshold and its knee, and the weight of what an up pass adds.
struct PassConstants
{
  float sourceTexelWidth = 0.0f;
  float sourceTexelHeight = 0.0f;
  float targetTexelWidth = 0.0f;
  float targetTexelHeight = 0.0f;
  float thresholdLuminance = 0.0f;
  float kneeLuminance = 0.0f;
  float weight = 0.0f;
  float unused0 = 0.0f;
};
constexpr UINT PASS_CONSTANT_COUNT = sizeof(PassConstants) / sizeof(float);

winrt::com_ptr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* _device)
{
  const CD3DX12_DESCRIPTOR_RANGE1 sourceRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
                                              D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
  std::array<CD3DX12_ROOT_PARAMETER1, 2> parameters{};
  parameters[SOURCE_PARAMETER].InitAsDescriptorTable(1, &sourceRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[PASS_PARAMETER].InitAsConstants(PASS_CONSTANT_COUNT, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  // Bilinear and clamped: the down and up passes take several taps each between texels, and none reads past the edge.
  const CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 16,
                                            D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE, 0.0f,
                                            D3D12_FLOAT32_MAX, D3D12_SHADER_VISIBILITY_PIXEL);

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC description;
  description.Init_1_1(static_cast<UINT>(parameters.size()), parameters.data(), 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

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
    throw winrt::hresult_error(result, winrt::to_hstring(std::format("The bloom root signature is invalid: {}", message)));
  }

  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(
    _device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(rootSignature)));
  return rootSignature;
}
} // namespace

std::array<Neuron::Bloom::Extent, Neuron::Bloom::LEVEL_COUNT> Neuron::Bloom::LevelExtents(UINT _widthPixels, UINT _heightPixels) noexcept
{
  std::array<Extent, LEVEL_COUNT> extents{};
  UINT width = _widthPixels;
  UINT height = _heightPixels;
  for (Extent& extent : extents)
  {
    width = std::max(1u, width / 2);
    height = std::max(1u, height / 2);
    extent = {.widthPixels = width, .heightPixels = height};
  }
  return extents;
}

float Neuron::Bloom::CompositeScale(const BloomDesc& _desc) noexcept
{
  // Each level after the first is weighed spread times the one before it, as the up passes add them.
  const float spread = std::clamp(_desc.spread, 0.0f, 1.0f);
  float weights = 0.0f;
  float weight = 1.0f;
  for (UINT level = 0; level < LEVEL_COUNT; ++level)
  {
    weights += weight;
    weight *= spread;
  }
  return std::max(0.0f, _desc.intensity) / weights;
}

Neuron::Bloom::Bloom(Renderer& _renderer)
  : m_renderer(_renderer)
{
  ID3D12Device* device = _renderer.Device();
  m_rootSignature = CreateRootSignature(device);

  // One triangle over the level written, as the resolve draws over the screen (ADR-050). Every pixel is written, so nothing
  // is tested or culled; only the up pass blends, adding to what the down pass left.
  CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
  rasterizer.CullMode = D3D12_CULL_MODE_NONE;
  CD3DX12_DEPTH_STENCIL_DESC depth(D3D12_DEFAULT);
  depth.DepthEnable = FALSE;
  depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  // Every member with an enum that has no zero value is set here, so none is ever left at an invalid zero.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC description{
    .pRootSignature = m_rootSignature.get(),
    .VS = CD3DX12_SHADER_BYTECODE(g_BloomVS, sizeof(g_BloomVS)),
    .PS = CD3DX12_SHADER_BYTECODE(g_BloomBrightPS, sizeof(g_BloomBrightPS)),
    .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
    .SampleMask = UINT_MAX,
    .RasterizerState = rasterizer,
    .DepthStencilState = depth,
    .InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0},
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = {FORMAT},
    .DSVFormat = DXGI_FORMAT_UNKNOWN,
    .SampleDesc = {.Count = 1, .Quality = 0},
  };
  winrt::check_hresult(device->CreateGraphicsPipelineState(&description, IID_GRAPHICS_PPV_ARGS(m_brightState)));
  description.PS = CD3DX12_SHADER_BYTECODE(g_BloomDownPS, sizeof(g_BloomDownPS));
  winrt::check_hresult(device->CreateGraphicsPipelineState(&description, IID_GRAPHICS_PPV_ARGS(m_downState)));

  CD3DX12_BLEND_DESC adding(D3D12_DEFAULT);
  adding.RenderTarget[0].BlendEnable = TRUE;
  adding.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
  adding.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
  adding.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  adding.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  adding.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
  adding.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  description.PS = CD3DX12_SHADER_BYTECODE(g_BloomUpPS, sizeof(g_BloomUpPS));
  description.BlendState = adding;
  winrt::check_hresult(device->CreateGraphicsPipelineState(&description, IID_GRAPHICS_PPV_ARGS(m_upState)));

  const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    .NumDescriptors = LEVEL_COUNT,
    .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    .NodeMask = 0,
  };
  winrt::check_hresult(device->CreateDescriptorHeap(&heapDescription, IID_GRAPHICS_PPV_ARGS(m_targetHeap)));
  m_targetViewBytes = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for (UINT& view : m_levelViews)
    view = _renderer.TakeShaderView();
}

void Neuron::Bloom::Resize(UINT _widthPixels, UINT _heightPixels)
{
  ID3D12Device* device = m_renderer.Device();
  m_extents = LevelExtents(_widthPixels, _heightPixels);
  const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
  for (UINT level = 0; level < LEVEL_COUNT; ++level)
  {
    // The old level is released first; the caller has drained the GPU. A level rests as a pixel shader resource, and every
    // pixel of it is written before it is read, so it is never cleared.
    m_levels[level] = nullptr;
    const CD3DX12_RESOURCE_DESC levelDescription = CD3DX12_RESOURCE_DESC::Tex2D(
      FORMAT, m_extents[level].widthPixels, m_extents[level].heightPixels, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    winrt::check_hresult(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &levelDescription,
                                                         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                                         IID_GRAPHICS_PPV_ARGS(m_levels[level])));

    D3D12_RENDER_TARGET_VIEW_DESC targetView{};
    targetView.Format = FORMAT;
    targetView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(m_levels[level].get(), &targetView, LevelTargetView(level));

    const D3D12_SHADER_RESOURCE_VIEW_DESC shaderView{
      .Format = FORMAT,
      .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
      .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
      .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0f}};
    device->CreateShaderResourceView(m_levels[level].get(), &shaderView, m_renderer.ShaderViewCpu(m_levelViews[level]));
  }
}

void Neuron::Bloom::Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_DESCRIPTOR_HANDLE _sceneView, const BloomDesc& _desc) const
{
  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // One pass: _state draws over level _target, reading _source, whose texels are _sourceExtent's.
  const auto pass =
    [&](UINT _target, ID3D12PipelineState* _state, D3D12_GPU_DESCRIPTOR_HANDLE _source, const Extent& _sourceExtent, float _weight)
  {
    ID3D12Resource* target = m_levels[_target].get();
    const CD3DX12_RESOURCE_BARRIER toTarget =
      CD3DX12_RESOURCE_BARRIER::Transition(target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    _commandList->ResourceBarrier(1, &toTarget);

    const Extent& extent = m_extents[_target];
    const D3D12_CPU_DESCRIPTOR_HANDLE targetView = LevelTargetView(_target);
    _commandList->OMSetRenderTargets(1, &targetView, FALSE, nullptr);
    const D3D12_VIEWPORT viewport{.TopLeftX = 0.0f,
                                  .TopLeftY = 0.0f,
                                  .Width = static_cast<float>(extent.widthPixels),
                                  .Height = static_cast<float>(extent.heightPixels),
                                  .MinDepth = D3D12_MIN_DEPTH,
                                  .MaxDepth = D3D12_MAX_DEPTH};
    const D3D12_RECT scissor{
      .left = 0, .top = 0, .right = static_cast<LONG>(extent.widthPixels), .bottom = static_cast<LONG>(extent.heightPixels)};
    _commandList->RSSetViewports(1, &viewport);
    _commandList->RSSetScissorRects(1, &scissor);

    const PassConstants constants{
      .sourceTexelWidth = _sourceExtent.widthPixels > 0 ? 1.0f / static_cast<float>(_sourceExtent.widthPixels) : 0.0f,
      .sourceTexelHeight = _sourceExtent.heightPixels > 0 ? 1.0f / static_cast<float>(_sourceExtent.heightPixels) : 0.0f,
      .targetTexelWidth = 1.0f / static_cast<float>(extent.widthPixels),
      .targetTexelHeight = 1.0f / static_cast<float>(extent.heightPixels),
      .thresholdLuminance = _desc.thresholdLuminance,
      .kneeLuminance = std::max(0.0f, _desc.kneeLuminance),
      .weight = _weight,
      .unused0 = 0.0f,
    };
    _commandList->SetPipelineState(_state);
    _commandList->SetGraphicsRootDescriptorTable(SOURCE_PARAMETER, _source);
    _commandList->SetGraphicsRoot32BitConstants(PASS_PARAMETER, PASS_CONSTANT_COUNT, &constants, 0);
    _commandList->DrawInstanced(3, 1, 0, 0);

    const CD3DX12_RESOURCE_BARRIER toShader =
      CD3DX12_RESOURCE_BARRIER::Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    _commandList->ResourceBarrier(1, &toShader);
  };

  // The scene's bright light into the first level; it reads the scene by whole pixels, so it needs no texel size.
  pass(0, m_brightState.get(), _sceneView, {}, 0.0f);
  // Down the chain, each level from the one above it.
  for (UINT level = 1; level < LEVEL_COUNT; ++level)
    pass(level, m_downState.get(), m_renderer.ShaderViewGpu(m_levelViews[level - 1]), m_extents[level - 1], 0.0f);
  // Back up, each level added into the one above it, weighed by the spread.
  const float spread = std::clamp(_desc.spread, 0.0f, 1.0f);
  for (UINT level = LEVEL_COUNT - 1; level > 0; --level)
    pass(level - 1, m_upState.get(), m_renderer.ShaderViewGpu(m_levelViews[level]), m_extents[level], spread);
}

D3D12_GPU_DESCRIPTOR_HANDLE Neuron::Bloom::FirstLevelView() const noexcept
{
  return m_renderer.ShaderViewGpu(m_levelViews[0]);
}

D3D12_CPU_DESCRIPTOR_HANDLE Neuron::Bloom::LevelTargetView(UINT _level) const noexcept
{
  return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_targetHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(_level), m_targetViewBytes);
}
