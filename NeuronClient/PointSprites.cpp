#include "pch.h"

#include "PointSprites.h"

// ADR-012: `dxc` compiles each stage into a header at build time and the header is checked in, so
// there is no file to read at device creation and a missing shader is a compile error.
#include "CompiledShader/StarPS.h"
#include "CompiledShader/StarVS.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>
#include <climits>
#include <cstring>

// `D3D12SerializeRootSignature` is a free function and therefore an import, as it is in `MeshPass.cpp`.
#pragma comment(lib, "d3d12.lib")

namespace Neuron
{

struct PointSpritesBinding
{
  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::com_ptr<ID3D12PipelineState> pipelineState;

  winrt::com_ptr<ID3D12Resource> instances;
  void* mapped = nullptr;
  std::uint32_t starCount = 0;

  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
inline constexpr std::uint32_t SKY_PARAMETER = 0;

/// One slot, stepping per instance. There is no per-vertex slot at all: the quad's four corners come
/// out of `SV_VertexID`, which is what makes this a sprite draw rather than a mesh draw.
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
} // namespace

PointSprites::PointSprites() noexcept
  : m_binding(std::make_shared<PointSpritesBinding>())
{
}

PointSprites::~PointSprites() noexcept = default;

bool PointSprites::Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget) noexcept
{
  PointSpritesBinding& binding = *m_binding;
  binding.ready = false;
  binding.lastHresult = S_OK;

  winrt::com_ptr<ID3D12Device> device;
  if (!OpenDevice(_device, device) || !_sceneTarget.IsReady())
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  std::array<D3D12_ROOT_PARAMETER, 1> parameters{};
  parameters[SKY_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[SKY_PARAMETER].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = SKY_CONSTANT_COUNT};
  parameters[SKY_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

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

  const std::array<D3D12_INPUT_ELEMENT_DESC, 3> elements{
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "TEXCOORD",
                             .SemanticIndex = 0,
                             .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                             .InputSlot = INSTANCE_SLOT,
                             .AlignedByteOffset = 0,
                             .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                             .InstanceDataStepRate = 1},
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "TEXCOORD",
                             .SemanticIndex = 1,
                             .Format = DXGI_FORMAT_R32_FLOAT,
                             .InputSlot = INSTANCE_SLOT,
                             .AlignedByteOffset = 12,
                             .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                             .InstanceDataStepRate = 1},
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "TEXCOORD",
                             .SemanticIndex = 2,
                             .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                             .InputSlot = INSTANCE_SLOT,
                             .AlignedByteOffset = 16,
                             .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                             .InstanceDataStepRate = 1}};

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDescription{};
  pipelineDescription.pRootSignature = binding.rootSignature.get();
  pipelineDescription.VS = {.pShaderBytecode = g_pStarVS, .BytecodeLength = sizeof(g_pStarVS)};
  pipelineDescription.PS = {.pShaderBytecode = g_pStarPS, .BytecodeLength = sizeof(g_pStarPS)};

  // **ADDITIVE, WHICH IS WHAT OVERLAPPING STARS ACTUALLY DO.** Source times one plus destination
  // times one: two faint sprites that overlap come out brighter than either, where an alpha blend
  // would let the later one cut a dark ring into the earlier.
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

  pipelineDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

  // **CULLING OFF**, for the reason `WorldPass` has it off: the quad is generated from a vertex
  // identifier, so its winding is whatever the strip's corner arithmetic produced rather than
  // something authored. Culling here would hide a sign error as an empty sky instead of a mirrored one.
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

  // **TESTED, NOT WRITTEN, AND `LESS_EQUAL` RATHER THAN `LESS`.** The test is what makes "draw it last"
  // cheap: a star behind a hull is rejected before it shades. The equality matters because the target
  // clears to exactly 1.0 and the shader emits exactly 1.0 -- a strict `LESS` rejects the entire sky,
  // which looks like the pass never having run. Writing depth would be pointless: nothing is drawn
  // after the sky, and a sky that wrote the far plane would change nothing except a bandwidth bill.
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

  binding.ready = true;
  return true;
}

void PointSprites::Destroy() noexcept
{
  PointSpritesBinding& binding = *m_binding;
  if ((binding.instances != nullptr) && (binding.mapped != nullptr))
  {
    binding.instances->Unmap(0, nullptr);
  }
  binding.mapped = nullptr;
  binding.instances = nullptr;
  binding.starCount = 0;
  binding.pipelineState = nullptr;
  binding.rootSignature = nullptr;
  binding.ready = false;
}

bool PointSprites::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t PointSprites::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

std::uint32_t PointSprites::StarCount() const noexcept
{
  return m_binding->starCount;
}

bool PointSprites::Upload(const GraphicsDevice& _device, const std::vector<StarInstance>& _stars) noexcept
{
  PointSpritesBinding& binding = *m_binding;

  winrt::com_ptr<ID3D12Device> device;
  if (!OpenDevice(_device, device))
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  if (_stars.empty())
  {
    binding.starCount = 0;
    return false;
  }

  // The previous field's buffer goes first. Uploading twice is a resolution change or a new match,
  // not a frame, so the reallocation is the honest thing rather than a pool.
  if ((binding.instances != nullptr) && (binding.mapped != nullptr))
  {
    binding.instances->Unmap(0, nullptr);
    binding.mapped = nullptr;
  }
  binding.instances = nullptr;

  const std::uint32_t bytes = static_cast<std::uint32_t>(_stars.size() * sizeof(StarInstance));

  const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_UPLOAD,
                                   .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                   .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                   .CreationNodeMask = 1,
                                   .VisibleNodeMask = 1};

  const D3D12_RESOURCE_DESC description{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                        .Alignment = 0,
                                        .Width = bytes,
                                        .Height = 1,
                                        .DepthOrArraySize = 1,
                                        .MipLevels = 1,
                                        .Format = DXGI_FORMAT_UNKNOWN,
                                        .SampleDesc = {.Count = 1, .Quality = 0},
                                        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                        .Flags = D3D12_RESOURCE_FLAG_NONE};

  binding.lastHresult = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                        nullptr, winrt::guid_of<ID3D12Resource>(), binding.instances.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  // A READ RANGE OF NOTHING, because nothing here reads back.
  const D3D12_RANGE noRead{.Begin = 0, .End = 0};
  binding.lastHresult = binding.instances->Map(0, &noRead, &binding.mapped);
  if (FAILED(binding.lastHresult) || (binding.mapped == nullptr))
  {
    binding.instances = nullptr;
    binding.mapped = nullptr;
    return false;
  }

  std::memcpy(binding.mapped, _stars.data(), bytes);
  binding.starCount = static_cast<std::uint32_t>(_stars.size());
  return true;
}

bool PointSprites::Draw(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, const float (&_viewRotationProjection)[16]) noexcept
{
  PointSpritesBinding& binding = *m_binding;
  if (!binding.ready || !_sceneTarget.IsReady() || (binding.starCount == 0) || (binding.instances == nullptr))
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  // Twenty constants: the matrix, then the target's size and two unused. The shader reads the size to
  // turn a sprite's pixels into normalized units, which is how a star keeps its size on the glass at
  // every world resolution (ADR-016).
  std::array<float, SKY_CONSTANT_COUNT> constants{};
  for (std::size_t index = 0; index < 16; ++index)
  {
    constants[index] = _viewRotationProjection[index];
  }
  constants[16] = static_cast<float>(_sceneTarget.WidthPixels());
  constants[17] = static_cast<float>(_sceneTarget.HeightPixels());

  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0f,
                                .TopLeftY = 0.0f,
                                .Width = static_cast<float>(_sceneTarget.WidthPixels()),
                                .Height = static_cast<float>(_sceneTarget.HeightPixels()),
                                .MinDepth = 0.0f,
                                .MaxDepth = 1.0f};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = _sceneTarget.WidthPixels(), .bottom = _sceneTarget.HeightPixels()};

  const D3D12_VERTEX_BUFFER_VIEW instanceView{.BufferLocation = binding.instances->GetGPUVirtualAddress(),
                                              .SizeInBytes = binding.starCount * static_cast<UINT>(sizeof(StarInstance)),
                                              .StrideInBytes = static_cast<UINT>(sizeof(StarInstance))};

  commandList->SetGraphicsRootSignature(binding.rootSignature.get());
  commandList->SetGraphicsRoot32BitConstants(SKY_PARAMETER, SKY_CONSTANT_COUNT, constants.data(), 0);
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetPipelineState(binding.pipelineState.get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  commandList->IASetVertexBuffers(INSTANCE_SLOT, 1, &instanceView);

  // **ONE CALL FOR THE WHOLE SKY.** Four vertices a sprite, three thousand sprites.
  commandList->DrawInstanced(VERTICES_PER_SPRITE, binding.starCount, 0, 0);
  return true;
}

} // namespace Neuron
