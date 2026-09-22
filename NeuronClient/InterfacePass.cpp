#include "pch.h"

#include "InterfacePass.h"

// ADR-012: `dxc` compiles each stage into a header at build time and the header is checked in, so
// there is no file to read at device creation and a missing shader is a compile error. One file per
// stage, and the symbol names come from `VariableName` in the project file.
#include "CompiledShader/InterfacePS.h"
#include "CompiledShader/InterfaceVS.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>
#include <climits>

// `D3D12SerializeRootSignature` is a free function and therefore an import, exactly as it is in
// `PresentStep.cpp`. The link dependency travels with the code that needs it.
#pragma comment(lib, "d3d12.lib")

namespace Neuron
{

struct InterfacePassBinding
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
inline constexpr std::uint32_t CLIP_RECT_PARAMETER = 0;
inline constexpr std::uint32_t COLOR_PARAMETER = 1;

/// A strip of four vertices, so one draw call and no index buffer. The vertex stage turns the
/// identifier into a corner of the root-constant rectangle; nothing is fetched.
inline constexpr std::uint32_t QUAD_VERTEX_COUNT = 4;

/// An authored coordinate onto a fitted axis. ROUNDED TO NEAREST, away from zero on a tie, and in
/// integers throughout: the fitted extent is what M0.12 already proved exact, so a ratio against it
/// cannot land at 1.99 where a float multiply can.
[[nodiscard]] std::int32_t MapAxis(std::int32_t _authored, std::int32_t _fittedExtent, std::int32_t _authoredExtent) noexcept
{
  if (_authoredExtent <= 0)
  {
    return 0;
  }

  const std::int64_t numerator = static_cast<std::int64_t>(_authored) * _fittedExtent;
  const std::int64_t half = _authoredExtent / 2;
  const std::int64_t rounded = (numerator >= 0) ? ((numerator + half) / _authoredExtent) : ((numerator - half) / _authoredExtent);
  return static_cast<std::int32_t>(rounded);
}

[[nodiscard]] bool OpenCommandList(const GraphicsDevice& _device, winrt::com_ptr<ID3D12GraphicsCommandList>& _outList) noexcept
{
  return (_device.CommandListUnknown() != nullptr) &&
         SUCCEEDED(_device.CommandListUnknown()->QueryInterface(winrt::guid_of<ID3D12GraphicsCommandList>(), _outList.put_void()));
}
} // namespace

PhysicalRect MapAuthoredRect(const FitTransform& _interfaceFit, const AuthoredRect& _rect) noexcept
{
  return PhysicalRect{.left = _interfaceFit.offsetX + MapAxis(_rect.left, _interfaceFit.width, INTERFACE_AUTHORED_WIDTH),
                      .top = _interfaceFit.offsetY + MapAxis(_rect.top, _interfaceFit.height, INTERFACE_AUTHORED_HEIGHT),
                      .right = _interfaceFit.offsetX + MapAxis(_rect.right, _interfaceFit.width, INTERFACE_AUTHORED_WIDTH),
                      .bottom = _interfaceFit.offsetY + MapAxis(_rect.bottom, _interfaceFit.height, INTERFACE_AUTHORED_HEIGHT)};
}

ClipRect ToClipRect(const PhysicalRect& _rect, std::int32_t _bufferWidthPixels, std::int32_t _bufferHeightPixels) noexcept
{
  if ((_bufferWidthPixels <= 0) || (_bufferHeightPixels <= 0))
  {
    return ClipRect{};
  }

  const float bufferWidth = static_cast<float>(_bufferWidthPixels);
  const float bufferHeight = static_cast<float>(_bufferHeightPixels);

  // THE Y AXIS FLIPS HERE AND NOWHERE ELSE. A back buffer counts rows downwards from the top and
  // clip space counts them upwards from the center, so `top` comes out the larger of the two.
  return ClipRect{.left = ((2.0f * static_cast<float>(_rect.left)) / bufferWidth) - 1.0f,
                  .top = 1.0f - ((2.0f * static_cast<float>(_rect.top)) / bufferHeight),
                  .right = ((2.0f * static_cast<float>(_rect.right)) / bufferWidth) - 1.0f,
                  .bottom = 1.0f - ((2.0f * static_cast<float>(_rect.bottom)) / bufferHeight)};
}

InterfacePass::InterfacePass() noexcept
  : m_binding{std::make_shared<InterfacePassBinding>()}
{
}

InterfacePass::~InterfacePass() noexcept
{
  Destroy();
}

bool InterfacePass::Create(const GraphicsDevice& _device, const SwapChain& _swapChain) noexcept
{
  InterfacePassBinding& binding = *m_binding;
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

  // Eight root constants and nothing else -- no table, no heap, no constant buffer. A root
  // parameter holds a union, which is why these are assigned rather than braced.
  std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
  parameters[CLIP_RECT_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[CLIP_RECT_PARAMETER].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = CLIP_RECT_CONSTANT_COUNT};
  parameters[CLIP_RECT_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  parameters[COLOR_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[COLOR_PARAMETER].Constants = {.ShaderRegister = 1, .RegisterSpace = 0, .Num32BitValues = COLOR_CONSTANT_COUNT};
  parameters[COLOR_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // No `ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT`: there is no vertex buffer and no input layout, and
  // claiming one costs a root signature slot for nothing.
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
  pipelineDescription.VS = {.pShaderBytecode = g_pInterfaceVS, .BytecodeLength = sizeof(g_pInterfaceVS)};
  pipelineDescription.PS = {.pShaderBytecode = g_pInterfacePS, .BytecodeLength = sizeof(g_pInterfacePS)};

  // STRAIGHT ALPHA, and it is the one state here that is a decision rather than a default: ADR-011
  // puts this pass over a back buffer the world has already been blitted into, so a plate or a text
  // quad composites with the scene rather than replacing it.
  //
  // THIS STEP DOES NOT PROVE IT WORKS. M0 draws one opaque rectangle, and at an alpha of one this
  // blend is arithmetically `ONE`/`ZERO` -- a mis-set factor is invisible until something is
  // actually translucent, which is the glyph atlas at M1.12.
  pipelineDescription.BlendState.AlphaToCoverageEnable = FALSE;
  pipelineDescription.BlendState.IndependentBlendEnable = FALSE;
  pipelineDescription.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pipelineDescription.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
  pipelineDescription.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipelineDescription.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  pipelineDescription.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipelineDescription.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  pipelineDescription.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  pipelineDescription.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipelineDescription.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
  pipelineDescription.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pipelineDescription.SampleMask = UINT_MAX;
  pipelineDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

  // NO CULLING, for the reason the present step gives: the quad's winding is whatever the vertex
  // identifier arithmetic produces, and culling would make the interface invisible for a reason
  // nobody would find.
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

  // The interface has no depth and neither has the back buffer it draws into. Order is the draw
  // order, which for a dozen rectangles is what it should be.
  pipelineDescription.DepthStencilState.DepthEnable = FALSE;
  pipelineDescription.DepthStencilState.StencilEnable = FALSE;
  pipelineDescription.InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0};
  pipelineDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipelineDescription.NumRenderTargets = 1;
  pipelineDescription.RTVFormats[0] = static_cast<DXGI_FORMAT>(_swapChain.BackBufferFormatCode());
  pipelineDescription.DSVFormat = DXGI_FORMAT_UNKNOWN;

  // ONE SAMPLE, and it describes the BACK BUFFER rather than the world: DXGI will not multisample a
  // flip-model back buffer at all, so the interface cannot be multisampled and ADR-011 says so.
  pipelineDescription.SampleDesc = {.Count = 1, .Quality = 0};

  binding.lastHresult =
    device->CreateGraphicsPipelineState(&pipelineDescription, winrt::guid_of<ID3D12PipelineState>(), binding.pipelineState.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.ready = true;
  return true;
}

void InterfacePass::Destroy() noexcept
{
  InterfacePassBinding& binding = *m_binding;
  binding.pipelineState = nullptr;
  binding.rootSignature = nullptr;
  binding.ready = false;
}

bool InterfacePass::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t InterfacePass::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

bool InterfacePass::Record(const GraphicsDevice& _device, const SwapChain& _swapChain, const FitTransform& _interfaceFit,
                           const InterfaceQuad& _quad) noexcept
{
  InterfacePassBinding& binding = *m_binding;
  if (!binding.ready || !_swapChain.IsReady())
  {
    return false;
  }

  const PhysicalRect placed = MapAuthoredRect(_interfaceFit, _quad.rect);
  if ((placed.WidthPixels() <= 0) || (placed.HeightPixels() <= 0))
  {
    // An empty rectangle is a caller's business rather than a fault: a panel that is not shown this
    // frame asks for nothing to be drawn, and nothing is.
    return true;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  // THE WHOLE BACK BUFFER, because the present step left the viewport on the letterboxed world
  // rectangle. The interface is positioned by its clip coordinates rather than by a viewport, so
  // this is a reset rather than a placement -- and it is what lets an element sit in a letterbox
  // bar, which is where an off-screen damage indicator goes (ADR-020).
  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0f,
                                .TopLeftY = 0.0f,
                                .Width = static_cast<float>(_swapChain.WidthPixels()),
                                .Height = static_cast<float>(_swapChain.HeightPixels()),
                                .MinDepth = 0.0f,
                                .MaxDepth = 1.0f};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = _swapChain.WidthPixels(), .bottom = _swapChain.HeightPixels()};

  const ClipRect clip = ToClipRect(placed, _swapChain.WidthPixels(), _swapChain.HeightPixels());
  const std::array<float, CLIP_RECT_CONSTANT_COUNT> clipConstants{clip.left, clip.top, clip.right, clip.bottom};
  const std::array<float, COLOR_CONSTANT_COUNT> colorConstants{_quad.red, _quad.green, _quad.blue, _quad.alpha};

  commandList->SetGraphicsRootSignature(binding.rootSignature.get());
  commandList->SetGraphicsRoot32BitConstants(CLIP_RECT_PARAMETER, CLIP_RECT_CONSTANT_COUNT, clipConstants.data(), 0);
  commandList->SetGraphicsRoot32BitConstants(COLOR_PARAMETER, COLOR_CONSTANT_COUNT, colorConstants.data(), 0);
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetPipelineState(binding.pipelineState.get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  commandList->DrawInstanced(QUAD_VERTEX_COUNT, 1, 0, 0);
  return true;
}

} // namespace Neuron
