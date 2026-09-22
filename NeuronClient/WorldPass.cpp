#include "pch.h"

#include "WorldPass.h"

// ADR-012: `dxc` compiles each stage into a header at build time and the header is checked in, so
// there is no file to read at device creation and a missing shader is a compile error.
#include "CompiledShader/WorldPS.h"
#include "CompiledShader/WorldVS.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>
#include <climits>

// `D3D12SerializeRootSignature` is a free function and therefore an import, as it is in
// `InterfacePass.cpp`. The link dependency travels with the code that needs it.
#pragma comment(lib, "d3d12.lib")

namespace Neuron
{

struct WorldPassBinding
{
  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::com_ptr<ID3D12PipelineState> pipelineState;

  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
/// Which root parameter is which. Two places rely on the order -- the signature is built in it and
/// `Record` writes into it -- so it is named rather than counted.
inline constexpr std::uint32_t TRANSFORM_PARAMETER = 0;
inline constexpr std::uint32_t COLOR_PARAMETER = 1;

[[nodiscard]] bool OpenCommandList(const GraphicsDevice& _device, winrt::com_ptr<ID3D12GraphicsCommandList>& _outList) noexcept
{
  return (_device.CommandListUnknown() != nullptr) &&
         SUCCEEDED(_device.CommandListUnknown()->QueryInterface(winrt::guid_of<ID3D12GraphicsCommandList>(), _outList.put_void()));
}
} // namespace

WorldPass::WorldPass() noexcept
  : m_binding(std::make_shared<WorldPassBinding>())
{
}

WorldPass::~WorldPass() noexcept = default;

bool WorldPass::Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget) noexcept
{
  WorldPassBinding& binding = *m_binding;
  binding.ready = false;
  binding.lastHresult = S_OK;

  if ((_device.State() != DeviceState::Ready) || !_sceneTarget.IsReady())
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

  // Twenty root constants and nothing else -- no table, no heap, no constant buffer. A root
  // parameter holds a union, which is why these are assigned rather than braced.
  std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
  parameters[TRANSFORM_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[TRANSFORM_PARAMETER].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = TRANSFORM_CONSTANT_COUNT};
  parameters[TRANSFORM_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  parameters[COLOR_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[COLOR_PARAMETER].Constants = {.ShaderRegister = 1, .RegisterSpace = 0, .Num32BitValues = COLOR_CONSTANT_COUNT};
  parameters[COLOR_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  const D3D12_ROOT_SIGNATURE_DESC rootDescription{.NumParameters = static_cast<UINT>(parameters.size()),
                                                  .pParameters = parameters.data(),
                                                  .NumStaticSamplers = 0,
                                                  .pStaticSamplers = nullptr,
                                                  .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                                           D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                                           D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS};

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

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDescription{};
  pipelineDescription.pRootSignature = binding.rootSignature.get();
  pipelineDescription.VS = {.pShaderBytecode = g_pWorldVS, .BytecodeLength = sizeof(g_pWorldVS)};
  pipelineDescription.PS = {.pShaderBytecode = g_pWorldPS, .BytecodeLength = sizeof(g_pWorldPS)};

  // OPAQUE. The world is silhouettes against black and nothing in it is translucent yet; the
  // interface pass is where blending lives (ADR-011).
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

  // NO CULLING, for the reason the other two passes give: the winding is whatever the vertex
  // identifier arithmetic produces, and a shape that is invisible because it faces away is a bug
  // nobody finds by looking at it. A camera that orbits underneath the plane would also flip it.
  pipelineDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipelineDescription.RasterizerState.FrontCounterClockwise = FALSE;
  pipelineDescription.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
  pipelineDescription.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
  pipelineDescription.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
  pipelineDescription.RasterizerState.DepthClipEnable = TRUE;
  pipelineDescription.RasterizerState.MultisampleEnable = (SceneTarget::SAMPLE_COUNT > 1) ? TRUE : FALSE;
  pipelineDescription.RasterizerState.AntialiasedLineEnable = FALSE;
  pipelineDescription.RasterizerState.ForcedSampleCount = 0;
  pipelineDescription.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  // DEPTH ON, AND THIS IS THE FIRST THING IN THE TREE TO WRITE TO IT. The scene target has carried
  // a depth buffer since M0.15 and nothing has used it. Two entities overlapping at tactical zoom
  // is the first thing that reads wrongly without it, and `LESS` rather than `LESS_EQUAL` because
  // nothing here draws twice at one depth.
  pipelineDescription.DepthStencilState.DepthEnable = TRUE;
  pipelineDescription.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  pipelineDescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
  pipelineDescription.DepthStencilState.StencilEnable = FALSE;
  pipelineDescription.InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0};
  pipelineDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipelineDescription.NumRenderTargets = 1;

  // Read off the target rather than restated -- a pipeline state that disagrees with what it renders
  // into is a device removal on a machine with the debug layer off.
  pipelineDescription.RTVFormats[0] = static_cast<DXGI_FORMAT>(_sceneTarget.ColorFormatCode());
  pipelineDescription.DSVFormat = static_cast<DXGI_FORMAT>(_sceneTarget.DepthFormatCode());
  pipelineDescription.SampleDesc = {.Count = SceneTarget::SAMPLE_COUNT, .Quality = 0};

  binding.lastHresult =
    device->CreateGraphicsPipelineState(&pipelineDescription, winrt::guid_of<ID3D12PipelineState>(), binding.pipelineState.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.ready = true;
  return true;
}

void WorldPass::Destroy() noexcept
{
  WorldPassBinding& binding = *m_binding;
  binding.pipelineState = nullptr;
  binding.rootSignature = nullptr;
  binding.ready = false;
}

bool WorldPass::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t WorldPass::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

bool WorldPass::Record(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, const float (&_worldViewProjection)[16], float _red,
                       float _green, float _blue, float _alpha) noexcept
{
  WorldPassBinding& binding = *m_binding;
  if (!binding.ready || !_sceneTarget.IsReady())
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  // THE SCENE TARGET'S FULL EXTENT, which is the world's resolution and not the window's
  // (ADR-016). The present step and the interface pass both leave the viewport somewhere else.
  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0f,
                                .TopLeftY = 0.0f,
                                .Width = static_cast<float>(_sceneTarget.WidthPixels()),
                                .Height = static_cast<float>(_sceneTarget.HeightPixels()),
                                .MinDepth = 0.0f,
                                .MaxDepth = 1.0f};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = _sceneTarget.WidthPixels(), .bottom = _sceneTarget.HeightPixels()};

  const std::array<float, COLOR_CONSTANT_COUNT> colorConstants{_red, _green, _blue, _alpha};

  commandList->SetGraphicsRootSignature(binding.rootSignature.get());
  commandList->SetGraphicsRoot32BitConstants(TRANSFORM_PARAMETER, TRANSFORM_CONSTANT_COUNT, _worldViewProjection, 0);
  commandList->SetGraphicsRoot32BitConstants(COLOR_PARAMETER, COLOR_CONSTANT_COUNT, colorConstants.data(), 0);
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetPipelineState(binding.pipelineState.get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  commandList->DrawInstanced(ARROW_VERTEX_COUNT, 1, 0, 0);
  return true;
}

} // namespace Neuron
