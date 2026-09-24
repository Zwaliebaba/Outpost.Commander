#include "pch.h"

#include "BeamPass.h"

// ADR-012: `dxc` compiles each stage into a header at build time and the header is checked in.
#include "CompiledShader/BeamPS.h"
#include "CompiledShader/BeamVS.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>

// `D3D12SerializeRootSignature` is a free function and therefore an import, as it is in `MeshPass.cpp`.
#pragma comment(lib, "d3d12.lib")

namespace Neuron
{

struct BeamPassBinding
{
  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::com_ptr<ID3D12PipelineState> pipelineState;

  /// One upload buffer a frame in flight, each mapped for its whole life.
  std::array<winrt::com_ptr<ID3D12Resource>, BeamPass::FRAME_COUNT> instances;
  std::array<void*, BeamPass::FRAME_COUNT> mapped{};
  std::uint32_t capacity = 0;

  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
inline constexpr std::uint32_t BEAM_PARAMETER = 0;
inline constexpr std::uint32_t INSTANCE_SLOT = 0;

[[nodiscard]] bool OpenCommandList(const GraphicsDevice& _device, winrt::com_ptr<ID3D12GraphicsCommandList>& _outList) noexcept
{
  return (_device.CommandListUnknown() != nullptr) &&
         SUCCEEDED(_device.CommandListUnknown()->QueryInterface(winrt::guid_of<ID3D12GraphicsCommandList>(), _outList.put_void()));
}

[[nodiscard]] bool OpenDevice(const GraphicsDevice& _device, winrt::com_ptr<ID3D12Device>& _outDevice) noexcept
{
  return (_device.State() == DeviceState::Ready) && (_device.DeviceUnknown() != nullptr) &&
         SUCCEEDED(_device.DeviceUnknown()->QueryInterface(winrt::guid_of<ID3D12Device>(), _outDevice.put_void()));
}

[[nodiscard]] D3D12_INPUT_ELEMENT_DESC InstanceElement(UINT _semanticIndex, DXGI_FORMAT _format, UINT _offsetBytes) noexcept
{
  return D3D12_INPUT_ELEMENT_DESC{.SemanticName = "TEXCOORD",
                                  .SemanticIndex = _semanticIndex,
                                  .Format = _format,
                                  .InputSlot = INSTANCE_SLOT,
                                  .AlignedByteOffset = _offsetBytes,
                                  .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                                  .InstanceDataStepRate = 1};
}
} // namespace

BeamPass::BeamPass() noexcept
  : m_binding(std::make_shared<BeamPassBinding>())
{
}

BeamPass::~BeamPass() noexcept
{
  Destroy();
}

bool BeamPass::Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, std::uint32_t _capacity) noexcept
{
  Destroy();
  BeamPassBinding& binding = *m_binding;
  binding.lastHresult = S_OK;

  winrt::com_ptr<ID3D12Device> device;
  if (!OpenDevice(_device, device) || !_sceneTarget.IsReady() || (_capacity == 0))
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  std::array<D3D12_ROOT_PARAMETER, 1> parameters{};
  parameters[BEAM_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[BEAM_PARAMETER].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = CONSTANT_COUNT};
  parameters[BEAM_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

  const D3D12_ROOT_SIGNATURE_DESC rootDescription{
    .NumParameters = static_cast<UINT>(parameters.size()),
    .pParameters = parameters.data(),
    .NumStaticSamplers = 0,
    .pStaticSamplers = nullptr,
    .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
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

  // The layout of `BeamInstance`: from and width, to and a pad, color and a pad.
  const std::array<D3D12_INPUT_ELEMENT_DESC, 4> elements{
    InstanceElement(0, DXGI_FORMAT_R32G32B32_FLOAT, 0), InstanceElement(1, DXGI_FORMAT_R32_FLOAT, 12),
    InstanceElement(2, DXGI_FORMAT_R32G32B32_FLOAT, 16), InstanceElement(3, DXGI_FORMAT_R32G32B32_FLOAT, 32)};

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDescription{};
  pipelineDescription.pRootSignature = binding.rootSignature.get();
  pipelineDescription.VS = {.pShaderBytecode = g_pBeamVS, .BytecodeLength = sizeof(g_pBeamVS)};
  pipelineDescription.PS = {.pShaderBytecode = g_pBeamPS, .BytecodeLength = sizeof(g_pBeamPS)};

  // **ADDITIVE**, as the stars are and for their reason: two beams that cross come out brighter than either.
  pipelineDescription.BlendState.AlphaToCoverageEnable = FALSE;
  pipelineDescription.BlendState.IndependentBlendEnable = FALSE;
  pipelineDescription.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pipelineDescription.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
  pipelineDescription.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
  pipelineDescription.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
  pipelineDescription.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipelineDescription.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
  pipelineDescription.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
  pipelineDescription.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipelineDescription.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
  pipelineDescription.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pipelineDescription.SampleMask = UINT_MAX;

  // **CULLING OFF**: the quad's winding follows the beam's direction, and a beam drawn right to left is as
  // much a beam as one drawn left to right.
  pipelineDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
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

  // **TESTED, NOT WRITTEN**: a hull in front of a beam hides it, and a beam hides nothing drawn after it.
  pipelineDescription.DepthStencilState.DepthEnable = TRUE;
  pipelineDescription.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  pipelineDescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  pipelineDescription.DepthStencilState.StencilEnable = FALSE;

  pipelineDescription.InputLayout = {.pInputElementDescs = elements.data(), .NumElements = static_cast<UINT>(elements.size())};
  pipelineDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipelineDescription.NumRenderTargets = 1;
  pipelineDescription.RTVFormats[0] = static_cast<DXGI_FORMAT>(_sceneTarget.ColorFormatCode());
  pipelineDescription.DSVFormat = static_cast<DXGI_FORMAT>(_sceneTarget.DepthFormatCode());
  pipelineDescription.SampleDesc = {.Count = SceneTarget::SAMPLE_COUNT, .Quality = 0};

  binding.lastHresult =
    device->CreateGraphicsPipelineState(&pipelineDescription, winrt::guid_of<ID3D12PipelineState>(), binding.pipelineState.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_UPLOAD,
                                   .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                   .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                   .CreationNodeMask = 1,
                                   .VisibleNodeMask = 1};

  const D3D12_RESOURCE_DESC description{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                        .Alignment = 0,
                                        .Width = static_cast<UINT64>(_capacity) * sizeof(BeamInstance),
                                        .Height = 1,
                                        .DepthOrArraySize = 1,
                                        .MipLevels = 1,
                                        .Format = DXGI_FORMAT_UNKNOWN,
                                        .SampleDesc = {.Count = 1, .Quality = 0},
                                        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                        .Flags = D3D12_RESOURCE_FLAG_NONE};

  // A READ RANGE OF NOTHING, because nothing here reads back.
  const D3D12_RANGE noRead{.Begin = 0, .End = 0};
  for (std::uint32_t frame = 0; frame < FRAME_COUNT; ++frame)
  {
    binding.lastHresult = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                          nullptr, winrt::guid_of<ID3D12Resource>(), binding.instances[frame].put_void());
    if (FAILED(binding.lastHresult))
    {
      Destroy();
      return false;
    }
    binding.lastHresult = binding.instances[frame]->Map(0, &noRead, &binding.mapped[frame]);
    if (FAILED(binding.lastHresult) || (binding.mapped[frame] == nullptr))
    {
      Destroy();
      return false;
    }
  }

  binding.capacity = _capacity;
  binding.ready = true;
  return true;
}

void BeamPass::Destroy() noexcept
{
  BeamPassBinding& binding = *m_binding;
  for (std::uint32_t frame = 0; frame < FRAME_COUNT; ++frame)
  {
    if ((binding.instances[frame] != nullptr) && (binding.mapped[frame] != nullptr))
    {
      binding.instances[frame]->Unmap(0, nullptr);
    }
    binding.mapped[frame] = nullptr;
    binding.instances[frame] = nullptr;
  }
  binding.capacity = 0;
  binding.pipelineState = nullptr;
  binding.rootSignature = nullptr;
  binding.ready = false;
}

bool BeamPass::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t BeamPass::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

bool BeamPass::Draw(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, std::uint32_t _frameIndex,
                    const float (&_viewProjection)[16], std::span<const BeamInstance> _beams) noexcept
{
  BeamPassBinding& binding = *m_binding;
  if (!binding.ready || !_sceneTarget.IsReady() || _beams.empty())
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  const std::uint32_t slot = _frameIndex % FRAME_COUNT;
  const std::uint32_t count = static_cast<std::uint32_t>(std::min<std::size_t>(_beams.size(), binding.capacity));
  std::memcpy(binding.mapped[slot], _beams.data(), static_cast<std::size_t>(count) * sizeof(BeamInstance));

  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0f,
                                .TopLeftY = 0.0f,
                                .Width = static_cast<float>(_sceneTarget.WidthPixels()),
                                .Height = static_cast<float>(_sceneTarget.HeightPixels()),
                                .MinDepth = 0.0f,
                                .MaxDepth = 1.0f};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = _sceneTarget.WidthPixels(), .bottom = _sceneTarget.HeightPixels()};

  const D3D12_VERTEX_BUFFER_VIEW instanceView{.BufferLocation = binding.instances[slot]->GetGPUVirtualAddress(),
                                              .SizeInBytes = count * static_cast<UINT>(sizeof(BeamInstance)),
                                              .StrideInBytes = static_cast<UINT>(sizeof(BeamInstance))};

  commandList->SetGraphicsRootSignature(binding.rootSignature.get());
  commandList->SetGraphicsRoot32BitConstants(BEAM_PARAMETER, CONSTANT_COUNT, _viewProjection, 0);
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetPipelineState(binding.pipelineState.get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  commandList->IASetVertexBuffers(INSTANCE_SLOT, 1, &instanceView);
  commandList->DrawInstanced(VERTICES_PER_BEAM, count, 0, 0);
  return true;
}

} // namespace Neuron
