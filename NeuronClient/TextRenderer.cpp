#include "pch.h"

#include "TextRenderer.h"

// ADR-012: `dxc` compiles each stage into a header at build time and the header is checked in.
#include "CompiledShader/GlyphPS.h"
#include "CompiledShader/GlyphVS.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>

// `D3D12SerializeRootSignature` is a free function and therefore an import, as it is in `InterfacePass.cpp`.
#pragma comment(lib, "d3d12.lib")

namespace Neuron
{

struct TextRendererBinding
{
  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::com_ptr<ID3D12PipelineState> pipelineState;

  /// One upload-heap buffer holding all three frames' worth, persistently mapped. Three regions of one
  /// resource rather than three resources, because nothing about them differs but the offset.
  winrt::com_ptr<ID3D12Resource> instances;
  std::byte* mapped = nullptr;

  std::uint32_t frame = 0;

  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
inline constexpr std::uint32_t TARGET_PARAMETER = 0;
inline constexpr std::uint32_t ATLAS_PARAMETER = 1;

inline constexpr std::uint32_t INSTANCE_SLOT = 0;
inline constexpr std::uint32_t QUAD_VERTEX_COUNT = 4;

inline constexpr std::uint64_t FRAME_BYTES = static_cast<std::uint64_t>(TextRenderer::MAXIMUM_QUADS) * sizeof(GlyphQuad);

[[nodiscard]] bool OpenDevice(const GraphicsDevice& _device, winrt::com_ptr<ID3D12Device>& _outDevice) noexcept
{
  return (_device.State() == DeviceState::Ready) && (_device.DeviceUnknown() != nullptr) &&
         SUCCEEDED(_device.DeviceUnknown()->QueryInterface(winrt::guid_of<ID3D12Device>(), _outDevice.put_void()));
}

[[nodiscard]] bool OpenCommandList(const GraphicsDevice& _device, winrt::com_ptr<ID3D12GraphicsCommandList>& _outList) noexcept
{
  return (_device.CommandListUnknown() != nullptr) &&
         SUCCEEDED(_device.CommandListUnknown()->QueryInterface(winrt::guid_of<ID3D12GraphicsCommandList>(), _outList.put_void()));
}
} // namespace

TextRenderer::TextRenderer() noexcept
  : m_binding(std::make_shared<TextRendererBinding>())
{
}

TextRenderer::~TextRenderer() noexcept
{
  Destroy();
}

bool TextRenderer::Create(const GraphicsDevice& _device, const SwapChain& _swapChain) noexcept
{
  TextRendererBinding& binding = *m_binding;
  binding.ready = false;
  binding.lastHresult = S_OK;

  winrt::com_ptr<ID3D12Device> device;
  if (!OpenDevice(_device, device) || !_swapChain.IsReady())
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  // The atlas is one table of one view; it is the atlas's heap, bound at draw time (see `Record`).
  const D3D12_DESCRIPTOR_RANGE atlasRange{.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                          .NumDescriptors = 1,
                                          .BaseShaderRegister = 0,
                                          .RegisterSpace = 0,
                                          .OffsetInDescriptorsFromTableStart = 0};

  std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
  parameters[TARGET_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[TARGET_PARAMETER].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = TARGET_CONSTANT_COUNT};
  parameters[TARGET_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  parameters[ATLAS_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[ATLAS_PARAMETER].DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &atlasRange};
  parameters[ATLAS_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // **POINT SAMPLING, CLAMPED.** See `GlyphPS.hlsl` for why point is right rather than cheap.
  const D3D12_STATIC_SAMPLER_DESC pointSampler{.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT,
                                               .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                               .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                               .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                               .MipLODBias = 0.0f,
                                               .MaxAnisotropy = 1,
                                               .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
                                               .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
                                               .MinLOD = 0.0f,
                                               .MaxLOD = 0.0f,
                                               .ShaderRegister = 0,
                                               .RegisterSpace = 0,
                                               .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL};

  const D3D12_ROOT_SIGNATURE_DESC rootDescription{
    .NumParameters = static_cast<UINT>(parameters.size()),
    .pParameters = parameters.data(),
    .NumStaticSamplers = 1,
    .pStaticSamplers = &pointSampler,
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

  // Three `float4`s a quad, stepping per instance. There is no per-vertex slot: the corners come from
  // `SV_VertexID`, as they do for the stars.
  const std::array<D3D12_INPUT_ELEMENT_DESC, 3> elements{
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "TEXCOORD",
                             .SemanticIndex = 0,
                             .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                             .InputSlot = INSTANCE_SLOT,
                             .AlignedByteOffset = 0,
                             .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                             .InstanceDataStepRate = 1},
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "TEXCOORD",
                             .SemanticIndex = 1,
                             .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                             .InputSlot = INSTANCE_SLOT,
                             .AlignedByteOffset = 16,
                             .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                             .InstanceDataStepRate = 1},
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "TEXCOORD",
                             .SemanticIndex = 2,
                             .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                             .InputSlot = INSTANCE_SLOT,
                             .AlignedByteOffset = 32,
                             .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                             .InstanceDataStepRate = 1}};

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDescription{};
  pipelineDescription.pRootSignature = binding.rootSignature.get();
  pipelineDescription.VS = {.pShaderBytecode = g_pGlyphVS, .BytecodeLength = sizeof(g_pGlyphVS)};
  pipelineDescription.PS = {.pShaderBytecode = g_pGlyphPS, .BytecodeLength = sizeof(g_pGlyphPS)};

  // **STRAIGHT ALPHA, AND THIS IS WHERE IT IS FIRST PROVED.** `InterfacePass` set the same blend at M0
  // and said so: at an alpha of one it is indistinguishable from no blend at all. A glyph's edge is the
  // first thing in this tree that is actually translucent.
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
  // No culling, for the reason every `SV_VertexID` quad in this tree gives.
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

  pipelineDescription.DepthStencilState.DepthEnable = FALSE;
  pipelineDescription.DepthStencilState.StencilEnable = FALSE;
  pipelineDescription.InputLayout = {.pInputElementDescs = elements.data(), .NumElements = static_cast<UINT>(elements.size())};
  pipelineDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipelineDescription.NumRenderTargets = 1;
  pipelineDescription.RTVFormats[0] = static_cast<DXGI_FORMAT>(_swapChain.BackBufferFormatCode());
  pipelineDescription.DSVFormat = DXGI_FORMAT_UNKNOWN;
  pipelineDescription.SampleDesc = {.Count = 1, .Quality = 0};

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
                                        .Width = FRAME_BYTES * FRAME_COUNT,
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

  const D3D12_RANGE noRead{.Begin = 0, .End = 0};
  void* mapped = nullptr;
  binding.lastHresult = binding.instances->Map(0, &noRead, &mapped);
  if (FAILED(binding.lastHresult) || (mapped == nullptr))
  {
    binding.instances = nullptr;
    return false;
  }
  binding.mapped = static_cast<std::byte*>(mapped);
  binding.frame = 0;

  binding.ready = true;
  return true;
}

void TextRenderer::Destroy() noexcept
{
  TextRendererBinding& binding = *m_binding;
  if ((binding.instances != nullptr) && (binding.mapped != nullptr))
  {
    binding.instances->Unmap(0, nullptr);
  }
  binding.mapped = nullptr;
  binding.instances = nullptr;
  binding.pipelineState = nullptr;
  binding.rootSignature = nullptr;
  binding.ready = false;
}

bool TextRenderer::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t TextRenderer::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

std::uint32_t TextRenderer::Record(const GraphicsDevice& _device, const SwapChain& _swapChain, const GlyphAtlas& _atlas,
                                   std::span<const GlyphQuad> _quads) noexcept
{
  TextRendererBinding& binding = *m_binding;
  if (!binding.ready || !_swapChain.IsReady() || !_atlas.IsReady() || _quads.empty() || (binding.mapped == nullptr))
  {
    return 0;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return 0;
  }

  const std::uint32_t count = static_cast<std::uint32_t>(std::min<std::size_t>(_quads.size(), MAXIMUM_QUADS));
  const std::uint64_t offset = FRAME_BYTES * binding.frame;
  std::memcpy(binding.mapped + offset, _quads.data(), static_cast<std::size_t>(count) * sizeof(GlyphQuad));
  binding.frame = (binding.frame + 1) % FRAME_COUNT;

  const std::array<float, TARGET_CONSTANT_COUNT> constants{static_cast<float>(_swapChain.WidthPixels()),
                                                           static_cast<float>(_swapChain.HeightPixels()), 0.0f, 0.0f};

  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0f,
                                .TopLeftY = 0.0f,
                                .Width = static_cast<float>(_swapChain.WidthPixels()),
                                .Height = static_cast<float>(_swapChain.HeightPixels()),
                                .MinDepth = 0.0f,
                                .MaxDepth = 1.0f};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = _swapChain.WidthPixels(), .bottom = _swapChain.HeightPixels()};

  const D3D12_VERTEX_BUFFER_VIEW instanceView{.BufferLocation = binding.instances->GetGPUVirtualAddress() + offset,
                                              .SizeInBytes = count * static_cast<UINT>(sizeof(GlyphQuad)),
                                              .StrideInBytes = static_cast<UINT>(sizeof(GlyphQuad))};

  // **THE ATLAS'S OWN HEAP**, which the atlas created with its view in it -- the arrangement
  // `PresentStep` has with `SceneTarget`. Setting it replaces whatever heap the present step left bound.
  ID3D12DescriptorHeap* const heaps[] = {static_cast<ID3D12DescriptorHeap*>(_atlas.ShaderResourceHeapUnknown())};
  const D3D12_GPU_DESCRIPTOR_HANDLE atlasHandle{.ptr = _atlas.ShaderResourceHandle()};

  commandList->SetGraphicsRootSignature(binding.rootSignature.get());
  commandList->SetDescriptorHeaps(1, heaps);
  commandList->SetGraphicsRoot32BitConstants(TARGET_PARAMETER, TARGET_CONSTANT_COUNT, constants.data(), 0);
  commandList->SetGraphicsRootDescriptorTable(ATLAS_PARAMETER, atlasHandle);
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetPipelineState(binding.pipelineState.get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  commandList->IASetVertexBuffers(INSTANCE_SLOT, 1, &instanceView);
  commandList->DrawInstanced(QUAD_VERTEX_COUNT, count, 0, 0);
  return count;
}

} // namespace Neuron
