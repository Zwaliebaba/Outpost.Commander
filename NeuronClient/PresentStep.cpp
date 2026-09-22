#include "pch.h"

#include "PresentStep.h"

// ADR-012: `dxc` compiles each stage into a header at build time and the header is checked in, so
// there is no file to read at device creation and a missing shader is a compile error. The symbol
// names come from `VariableName` in the project file, which is the one place this tree's naming
// convention has to be applied to generated code by hand.
#include "CompiledShader/PresentPS.h"
#include "CompiledShader/PresentVS.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>
#include <climits>

// `D3D12SerializeRootSignature` is a free function and therefore an import; every other Direct3D
// call in this file goes through an interface. The link dependency travels with the code that needs
// it, for the reason GraphicsDevice gives.
#pragma comment(lib, "d3d12.lib")

namespace Neuron
{

struct PresentStepBinding
{
  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::com_ptr<ID3D12PipelineState> pipelineState;
  winrt::com_ptr<ID3D12DescriptorHeap> samplerHeap;

  D3D12_GPU_DESCRIPTOR_HANDLE samplerHeapStart{};
  std::uint32_t samplerDescriptorSize = 0;
  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
/// Which descriptor in the sampler heap is which. The order is a fact two places rely on -- the
/// heap is filled in it and `SamplerIndex` reads it back -- so it is named rather than counted.
inline constexpr std::uint32_t POINT_SAMPLER_INDEX = 0;
inline constexpr std::uint32_t LINEAR_SAMPLER_INDEX = 1;

/// Which root parameter is which, for the same reason.
inline constexpr std::uint32_t SCENE_TEXTURE_PARAMETER = 0;
inline constexpr std::uint32_t SAMPLER_PARAMETER = 1;

/// R13's three cases arrive as two samplers. `None` is 1:1, where a point sample lands exactly on
/// the texel center and is the unfiltered copy the rule asks for; `Point` is the exact integer
/// multiple; only `Bilinear` resamples. Nothing here compares a float to a whole number, because
/// the decision was already taken in integers three steps ago (M0.12).
[[nodiscard]] std::uint32_t SamplerIndex(FitFilter _filter) noexcept
{
  return (_filter == FitFilter::Bilinear) ? LINEAR_SAMPLER_INDEX : POINT_SAMPLER_INDEX;
}

[[nodiscard]] bool OpenCommandList(const GraphicsDevice& _device, winrt::com_ptr<ID3D12GraphicsCommandList>& _outList) noexcept
{
  return (_device.CommandListUnknown() != nullptr) &&
         SUCCEEDED(_device.CommandListUnknown()->QueryInterface(winrt::guid_of<ID3D12GraphicsCommandList>(), _outList.put_void()));
}
} // namespace

PresentStep::PresentStep() noexcept
  : m_binding{std::make_shared<PresentStepBinding>()}
{
}

PresentStep::~PresentStep() noexcept
{
  Destroy();
}

bool PresentStep::Create(const GraphicsDevice& _device, const SwapChain& _swapChain) noexcept
{
  PresentStepBinding& binding = *m_binding;
  binding.ready = false;
  binding.lastHresult = S_OK;

  if ((_device.State() != DeviceState::Ready) || !_swapChain.IsReady())
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  winrt::com_ptr<ID3D12Device> device;
  if (FAILED(_device.DeviceUnknown()->QueryInterface(winrt::guid_of<ID3D12Device>(), device.put_void())))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  // One texture and one sampler, each behind a table of one, and the vertex stage denied both: the
  // full-screen triangle is generated from `SV_VertexID` and reads nothing at all.
  std::array<D3D12_DESCRIPTOR_RANGE, 2> ranges{};
  ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[0].NumDescriptors = 1;
  ranges[0].BaseShaderRegister = 0;
  ranges[0].RegisterSpace = 0;
  ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  ranges[1].NumDescriptors = 1;
  ranges[1].BaseShaderRegister = 0;
  ranges[1].RegisterSpace = 0;
  ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // A root parameter holds a union, which is why these are assigned rather than braced.
  std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
  parameters[SCENE_TEXTURE_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[SCENE_TEXTURE_PARAMETER].DescriptorTable.NumDescriptorRanges = 1;
  parameters[SCENE_TEXTURE_PARAMETER].DescriptorTable.pDescriptorRanges = &ranges[0];
  parameters[SCENE_TEXTURE_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  parameters[SAMPLER_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[SAMPLER_PARAMETER].DescriptorTable.NumDescriptorRanges = 1;
  parameters[SAMPLER_PARAMETER].DescriptorTable.pDescriptorRanges = &ranges[1];
  parameters[SAMPLER_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // No `ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT`: there is no vertex buffer and no input layout, and
  // claiming one costs a root signature slot for nothing.
  const D3D12_ROOT_SIGNATURE_DESC rootDescription{
    .NumParameters = static_cast<UINT>(parameters.size()),
    .pParameters = parameters.data(),
    .NumStaticSamplers = 0,
    .pStaticSamplers = nullptr,
    .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
             D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS};

  winrt::com_ptr<ID3DBlob> serialized;
  winrt::com_ptr<ID3DBlob> serializeError;
  binding.lastHresult = D3D12SerializeRootSignature(&rootDescription, D3D_ROOT_SIGNATURE_VERSION_1, serialized.put(), serializeError.put());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.lastHresult = device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                                    winrt::guid_of<ID3D12RootSignature>(), binding.rootSignature.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  const D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDescription{.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                                                          .NumDescriptors = SAMPLER_COUNT,
                                                          .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                                                          .NodeMask = 0};
  binding.lastHresult =
    device->CreateDescriptorHeap(&samplerHeapDescription, winrt::guid_of<ID3D12DescriptorHeap>(), binding.samplerHeap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.samplerDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  // CLAMP on every axis, because the fitted rectangle is exactly the texture: a bilinear tap at the
  // outermost pixel would otherwise wrap a row in from the far edge.
  D3D12_SAMPLER_DESC sampler{};
  sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.MipLODBias = 0.0f;
  sampler.MaxAnisotropy = 1;
  sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  sampler.MinLOD = 0.0f;

  // One mip, so there is nothing to select between. A scene target with a mip chain would be a
  // different decision and would need one.
  sampler.MaxLOD = 0.0f;

  std::array<D3D12_FILTER, SAMPLER_COUNT> filters{};
  filters[POINT_SAMPLER_INDEX] = D3D12_FILTER_MIN_MAG_MIP_POINT;
  filters[LINEAR_SAMPLER_INDEX] = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

  D3D12_CPU_DESCRIPTOR_HANDLE samplerHandle = binding.samplerHeap->GetCPUDescriptorHandleForHeapStart();
  for (const D3D12_FILTER filter : filters)
  {
    sampler.Filter = filter;
    device->CreateSampler(&sampler, samplerHandle);
    samplerHandle.ptr += binding.samplerDescriptorSize;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDescription{};
  pipelineDescription.pRootSignature = binding.rootSignature.get();
  pipelineDescription.VS = {.pShaderBytecode = g_pPresentVS, .BytecodeLength = sizeof(g_pPresentVS)};
  pipelineDescription.PS = {.pShaderBytecode = g_pPresentPS, .BytecodeLength = sizeof(g_pPresentPS)};
  pipelineDescription.BlendState.AlphaToCoverageEnable = FALSE;
  pipelineDescription.BlendState.IndependentBlendEnable = FALSE;
  pipelineDescription.BlendState.RenderTarget[0].BlendEnable = FALSE;
  pipelineDescription.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
  pipelineDescription.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
  pipelineDescription.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
  pipelineDescription.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipelineDescription.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  pipelineDescription.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  pipelineDescription.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipelineDescription.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
  pipelineDescription.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pipelineDescription.SampleMask = UINT_MAX;
  pipelineDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

  // NO CULLING, which is the one rasterizer setting here that is a decision rather than a default:
  // the full-screen triangle is generated from a vertex identifier and its winding is whatever that
  // arithmetic produces, so culling would make the frame blank for a reason nobody would find.
  pipelineDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipelineDescription.RasterizerState.FrontCounterClockwise = FALSE;
  pipelineDescription.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
  pipelineDescription.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
  pipelineDescription.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
  pipelineDescription.RasterizerState.DepthClipEnable = TRUE;
  pipelineDescription.RasterizerState.MultisampleEnable = FALSE;
  pipelineDescription.RasterizerState.AntialiasedLineEnable = FALSE;
  pipelineDescription.RasterizerState.ForcedSampleCount = 0;
  pipelineDescription.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  // The blit has no depth and neither has the back buffer it draws into. The scene target's depth
  // buffer belongs to the passes that fill it, not to the one that copies it out.
  pipelineDescription.DepthStencilState.DepthEnable = FALSE;
  pipelineDescription.DepthStencilState.StencilEnable = FALSE;
  pipelineDescription.InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0};
  pipelineDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipelineDescription.NumRenderTargets = 1;
  pipelineDescription.RTVFormats[0] = static_cast<DXGI_FORMAT>(_swapChain.BackBufferFormatCode());
  pipelineDescription.DSVFormat = DXGI_FORMAT_UNKNOWN;

  // ONE SAMPLE, and this one is not the world's constant: it describes the BACK BUFFER, which DXGI
  // will not multisample under the flip model at all (ADR-016).
  pipelineDescription.SampleDesc = {.Count = 1, .Quality = 0};

  binding.lastHresult =
    device->CreateGraphicsPipelineState(&pipelineDescription, winrt::guid_of<ID3D12PipelineState>(), binding.pipelineState.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.samplerHeapStart = binding.samplerHeap->GetGPUDescriptorHandleForHeapStart();
  binding.ready = true;
  return true;
}

void PresentStep::Destroy() noexcept
{
  PresentStepBinding& binding = *m_binding;
  binding.pipelineState = nullptr;
  binding.samplerHeap = nullptr;
  binding.rootSignature = nullptr;
  binding.samplerHeapStart = {};
  binding.samplerDescriptorSize = 0;
  binding.ready = false;
}

bool PresentStep::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t PresentStep::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

bool PresentStep::Record(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, const FitTransform& _fit) noexcept
{
  PresentStepBinding& binding = *m_binding;
  if (!binding.ready || !_sceneTarget.IsReady() || (_fit.width <= 0) || (_fit.height <= 0))
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  winrt::com_ptr<ID3D12Resource> sceneColor;
  winrt::com_ptr<ID3D12DescriptorHeap> sceneHeap;
  if (!OpenCommandList(_device, commandList) ||
      FAILED(_sceneTarget.ColorResourceUnknown()->QueryInterface(winrt::guid_of<ID3D12Resource>(), sceneColor.put_void())) ||
      FAILED(_sceneTarget.ShaderResourceHeapUnknown()->QueryInterface(winrt::guid_of<ID3D12DescriptorHeap>(), sceneHeap.put_void())))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  // RENDER_TARGET to PIXEL_SHADER_RESOURCE and back again in the same recording, so the scene
  // target is in one state at every frame boundary and the frame after this one needs no special
  // case. This is the only place that state moves.
  D3D12_RESOURCE_BARRIER barrier{.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                                 .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                                 .Transition = {.pResource = sceneColor.get(),
                                                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                                .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE}};
  commandList->ResourceBarrier(1, &barrier);

  // THE VIEWPORT IS THE LETTERBOX. The fitted rectangle came from M0.12 in integers; restricting
  // the viewport to it is what leaves the bars the back buffer's clear color, and it is why nothing
  // in this file compares an aspect ratio or branches on the window size.
  const D3D12_VIEWPORT viewport{.TopLeftX = static_cast<float>(_fit.offsetX),
                                .TopLeftY = static_cast<float>(_fit.offsetY),
                                .Width = static_cast<float>(_fit.width),
                                .Height = static_cast<float>(_fit.height),
                                .MinDepth = 0.0f,
                                .MaxDepth = 1.0f};
  const D3D12_RECT scissor{
    .left = _fit.offsetX, .top = _fit.offsetY, .right = _fit.offsetX + _fit.width, .bottom = _fit.offsetY + _fit.height};

  // Two heaps of two different types, which is what the one-heap-per-type rule allows: the scene
  // target owns the view of itself and this step owns the samplers.
  std::array<ID3D12DescriptorHeap*, 2> heaps{sceneHeap.get(), binding.samplerHeap.get()};
  commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
  commandList->SetGraphicsRootSignature(binding.rootSignature.get());

  const D3D12_GPU_DESCRIPTOR_HANDLE sceneView{.ptr = _sceneTarget.ShaderResourceHandle()};
  commandList->SetGraphicsRootDescriptorTable(SCENE_TEXTURE_PARAMETER, sceneView);

  D3D12_GPU_DESCRIPTOR_HANDLE samplerView = binding.samplerHeapStart;
  samplerView.ptr += static_cast<UINT64>(SamplerIndex(_fit.filter)) * binding.samplerDescriptorSize;
  commandList->SetGraphicsRootDescriptorTable(SAMPLER_PARAMETER, samplerView);

  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetPipelineState(binding.pipelineState.get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // ONE TRIANGLE THAT OVERHANGS, not two that meet. A quad's two triangles share a diagonal seam
  // down the middle of the screen, where the rasterizer shades the pixels either side of it twice.
  commandList->DrawInstanced(3, 1, 0, 0);

  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  commandList->ResourceBarrier(1, &barrier);
  return true;
}

} // namespace Neuron
