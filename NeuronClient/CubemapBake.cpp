#include "pch.h"

#include "CubemapBake.h"

// ADR-012: `dxc` compiles each stage into a header at build time and the header is checked in, so
// there is no file to read at device creation and a missing shader is a compile error.
#include "CompiledShader/GalaxyPS.h"
#include "CompiledShader/GalaxyVS.h"
#include "CompiledShader/SkyPS.h"
#include "CompiledShader/SkyVS.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>
#include <climits>

// `D3D12SerializeRootSignature` is a free function and therefore an import, as it is in `MeshPass.cpp`.
#pragma comment(lib, "d3d12.lib")

namespace Neuron
{

struct CubemapBakeBinding
{
  winrt::com_ptr<ID3D12Resource> cubemap;
  winrt::com_ptr<ID3D12DescriptorHeap> renderTargetHeap;
  winrt::com_ptr<ID3D12DescriptorHeap> shaderResourceHeap;
  winrt::com_ptr<ID3D12DescriptorHeap> samplerHeap;

  winrt::com_ptr<ID3D12RootSignature> bakeRootSignature;
  winrt::com_ptr<ID3D12PipelineState> bakePipelineState;

  winrt::com_ptr<ID3D12RootSignature> backdropRootSignature;
  winrt::com_ptr<ID3D12PipelineState> backdropPipelineState;

  std::uint32_t renderTargetStride = 0;

  HRESULT lastHresult = S_OK;
  bool ready = false;
  bool baked = false;
};

namespace
{
/// Four bytes a texel, which is what makes six 512-square faces 6.3 MB rather than 12.6. A packed
/// float rather than an 8-bit format because the band's luminance runs to a tenth of white and
/// banding in a smooth gradient that dark is exactly what an 8-bit channel gives.
inline constexpr DXGI_FORMAT CUBEMAP_FORMAT = DXGI_FORMAT_R11G11B10_FLOAT;

inline constexpr std::uint32_t FACE_PARAMETER = 0;
inline constexpr std::uint32_t GALAXY_PARAMETER = 1;

inline constexpr std::uint32_t RAY_PARAMETER = 0;
inline constexpr std::uint32_t CUBEMAP_PARAMETER = 1;
inline constexpr std::uint32_t SAMPLER_PARAMETER = 2;

/// **THE SIX FACE BASES, AND THEY ARE A CONVENTION RATHER THAN A CHOICE.** Direct3D orders a cubemap
/// +X, -X, +Y, -Y, +Z, -Z, and each face's horizontal and vertical axes are fixed by that order -- get
/// one wrong and the band is continuous across five seams and broken across the sixth, which is a
/// failure that only shows when the camera happens to look at that seam.
///
/// Each row is right, then up, then forward; the shader builds `forward + right * u + up * v` with u
/// and v running minus one to plus one across the face.
struct FaceBasis
{
  float right[3];
  float up[3];
  float forward[3];
};

inline constexpr std::array<FaceBasis, CubemapBake::FACE_COUNT> FACE_BASES{
  FaceBasis{{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // +X
  FaceBasis{{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},  // -X
  FaceBasis{{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},  // +Y
  FaceBasis{{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},  // -Y
  FaceBasis{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},   // +Z
  FaceBasis{{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}}, // -Z
};

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

CubemapBake::CubemapBake() noexcept
  : m_binding(std::make_shared<CubemapBakeBinding>())
{
}

CubemapBake::~CubemapBake() noexcept = default;

bool CubemapBake::Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget) noexcept
{
  CubemapBakeBinding& binding = *m_binding;
  binding.ready = false;
  binding.baked = false;
  binding.lastHresult = S_OK;

  winrt::com_ptr<ID3D12Device> device;
  if (!OpenDevice(_device, device) || !_sceneTarget.IsReady())
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  // === THE TEXTURE ==================================================================== //
  //
  // A cubemap in Direct3D is a two-dimensional array of six slices with a cube view over it; there is
  // no cube dimension on the resource itself.
  const D3D12_HEAP_PROPERTIES defaultHeap{.Type = D3D12_HEAP_TYPE_DEFAULT,
                                          .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                          .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                          .CreationNodeMask = 1,
                                          .VisibleNodeMask = 1};

  const D3D12_RESOURCE_DESC description{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                        .Alignment = 0,
                                        .Width = FACE_PIXELS,
                                        .Height = FACE_PIXELS,
                                        .DepthOrArraySize = static_cast<UINT16>(FACE_COUNT),
                                        .MipLevels = 1,
                                        .Format = CUBEMAP_FORMAT,
                                        .SampleDesc = {.Count = 1, .Quality = 0},
                                        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                        .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET};

  D3D12_CLEAR_VALUE clearValue{};
  clearValue.Format = CUBEMAP_FORMAT;
  clearValue.Color[0] = 0.0f;
  clearValue.Color[1] = 0.0f;
  clearValue.Color[2] = 0.0f;
  clearValue.Color[3] = 1.0f;

  binding.lastHresult =
    device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    &clearValue, winrt::guid_of<ID3D12Resource>(), binding.cubemap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  // === THE VIEWS ====================================================================== //

  const D3D12_DESCRIPTOR_HEAP_DESC renderTargetHeapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .NumDescriptors = FACE_COUNT, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE, .NodeMask = 0};
  binding.lastHresult =
    device->CreateDescriptorHeap(&renderTargetHeapDescription, winrt::guid_of<ID3D12DescriptorHeap>(), binding.renderTargetHeap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.renderTargetStride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  // One view per slice. Rendering to a cube face IS rendering to an array slice.
  D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle = binding.renderTargetHeap->GetCPUDescriptorHandleForHeapStart();
  for (std::uint32_t face = 0; face < FACE_COUNT; ++face)
  {
    D3D12_RENDER_TARGET_VIEW_DESC renderTargetView{};
    renderTargetView.Format = CUBEMAP_FORMAT;
    renderTargetView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    renderTargetView.Texture2DArray.MipSlice = 0;
    renderTargetView.Texture2DArray.FirstArraySlice = face;
    renderTargetView.Texture2DArray.ArraySize = 1;
    renderTargetView.Texture2DArray.PlaneSlice = 0;

    device->CreateRenderTargetView(binding.cubemap.get(), &renderTargetView, renderTargetHandle);
    renderTargetHandle.ptr += binding.renderTargetStride;
  }

  const D3D12_DESCRIPTOR_HEAP_DESC shaderResourceHeapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .NumDescriptors = 1, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, .NodeMask = 0};
  binding.lastHresult = device->CreateDescriptorHeap(&shaderResourceHeapDescription, winrt::guid_of<ID3D12DescriptorHeap>(),
                                                     binding.shaderResourceHeap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC shaderView{};
  shaderView.Format = CUBEMAP_FORMAT;
  shaderView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
  shaderView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  shaderView.TextureCube.MostDetailedMip = 0;
  shaderView.TextureCube.MipLevels = 1;
  shaderView.TextureCube.ResourceMinLODClamp = 0.0f;
  device->CreateShaderResourceView(binding.cubemap.get(), &shaderView, binding.shaderResourceHeap->GetCPUDescriptorHandleForHeapStart());

  const D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, .NumDescriptors = 1, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, .NodeMask = 0};
  binding.lastHresult =
    device->CreateDescriptorHeap(&samplerHeapDescription, winrt::guid_of<ID3D12DescriptorHeap>(), binding.samplerHeap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  // **LINEAR AND CLAMPED.** A point sample of a 512-square face at this field of view shows its texels
  // as blocks in the band's smooth gradient; clamping is what stops a fetch near an edge wrapping to
  // the far side of the face instead of crossing the seam.
  D3D12_SAMPLER_DESC sampler{};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.MipLODBias = 0.0f;
  sampler.MaxAnisotropy = 1;
  sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  sampler.MinLOD = 0.0f;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;
  device->CreateSampler(&sampler, binding.samplerHeap->GetCPUDescriptorHandleForHeapStart());

  // === THE BAKE'S PIPELINE ============================================================ //

  std::array<D3D12_ROOT_PARAMETER, 2> bakeParameters{};
  bakeParameters[FACE_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  bakeParameters[FACE_PARAMETER].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = FACE_CONSTANT_COUNT};
  bakeParameters[FACE_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  bakeParameters[GALAXY_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  bakeParameters[GALAXY_PARAMETER].Constants = {.ShaderRegister = 1, .RegisterSpace = 0, .Num32BitValues = GALAXY_CONSTANT_COUNT};
  bakeParameters[GALAXY_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  const D3D12_ROOT_SIGNATURE_DESC bakeRootDescription{.NumParameters = static_cast<UINT>(bakeParameters.size()),
                                                      .pParameters = bakeParameters.data(),
                                                      .NumStaticSamplers = 0,
                                                      .pStaticSamplers = nullptr,
                                                      .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                                               D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                                               D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS};

  winrt::com_ptr<ID3DBlob> serialized;
  winrt::com_ptr<ID3DBlob> serializeError;
  binding.lastHresult =
    D3D12SerializeRootSignature(&bakeRootDescription, D3D_ROOT_SIGNATURE_VERSION_1, serialized.put(), serializeError.put());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.lastHresult = device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                                    winrt::guid_of<ID3D12RootSignature>(), binding.bakeRootSignature.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC bakeDescription{};
  bakeDescription.pRootSignature = binding.bakeRootSignature.get();
  bakeDescription.VS = {.pShaderBytecode = g_pGalaxyVS, .BytecodeLength = sizeof(g_pGalaxyVS)};
  bakeDescription.PS = {.pShaderBytecode = g_pGalaxyPS, .BytecodeLength = sizeof(g_pGalaxyPS)};
  bakeDescription.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
  bakeDescription.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
  bakeDescription.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  bakeDescription.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  bakeDescription.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  bakeDescription.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  bakeDescription.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
  bakeDescription.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  bakeDescription.SampleMask = UINT_MAX;
  bakeDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  bakeDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  bakeDescription.RasterizerState.DepthClipEnable = TRUE;
  bakeDescription.DepthStencilState.DepthEnable = FALSE;
  bakeDescription.DepthStencilState.StencilEnable = FALSE;
  bakeDescription.InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0};
  bakeDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  bakeDescription.NumRenderTargets = 1;
  bakeDescription.RTVFormats[0] = CUBEMAP_FORMAT;
  bakeDescription.DSVFormat = DXGI_FORMAT_UNKNOWN;
  bakeDescription.SampleDesc = {.Count = 1, .Quality = 0};

  binding.lastHresult =
    device->CreateGraphicsPipelineState(&bakeDescription, winrt::guid_of<ID3D12PipelineState>(), binding.bakePipelineState.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  // === THE BACKDROP'S PIPELINE ======================================================== //

  std::array<D3D12_DESCRIPTOR_RANGE, 1> cubemapRange{};
  cubemapRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  cubemapRange[0].NumDescriptors = 1;
  cubemapRange[0].BaseShaderRegister = 0;
  cubemapRange[0].RegisterSpace = 0;
  cubemapRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  std::array<D3D12_DESCRIPTOR_RANGE, 1> samplerRange{};
  samplerRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  samplerRange[0].NumDescriptors = 1;
  samplerRange[0].BaseShaderRegister = 0;
  samplerRange[0].RegisterSpace = 0;
  samplerRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  std::array<D3D12_ROOT_PARAMETER, 3> backdropParameters{};
  backdropParameters[RAY_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  backdropParameters[RAY_PARAMETER].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = RAY_CONSTANT_COUNT};
  backdropParameters[RAY_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  backdropParameters[CUBEMAP_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  backdropParameters[CUBEMAP_PARAMETER].DescriptorTable = {.NumDescriptorRanges = static_cast<UINT>(cubemapRange.size()),
                                                           .pDescriptorRanges = cubemapRange.data()};
  backdropParameters[CUBEMAP_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  backdropParameters[SAMPLER_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  backdropParameters[SAMPLER_PARAMETER].DescriptorTable = {.NumDescriptorRanges = static_cast<UINT>(samplerRange.size()),
                                                           .pDescriptorRanges = samplerRange.data()};
  backdropParameters[SAMPLER_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  const D3D12_ROOT_SIGNATURE_DESC backdropRootDescription{.NumParameters = static_cast<UINT>(backdropParameters.size()),
                                                          .pParameters = backdropParameters.data(),
                                                          .NumStaticSamplers = 0,
                                                          .pStaticSamplers = nullptr,
                                                          .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                                                   D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                                                   D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS};

  winrt::com_ptr<ID3DBlob> backdropSerialized;
  winrt::com_ptr<ID3DBlob> backdropError;
  binding.lastHresult =
    D3D12SerializeRootSignature(&backdropRootDescription, D3D_ROOT_SIGNATURE_VERSION_1, backdropSerialized.put(), backdropError.put());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.lastHresult = device->CreateRootSignature(0, backdropSerialized->GetBufferPointer(), backdropSerialized->GetBufferSize(),
                                                    winrt::guid_of<ID3D12RootSignature>(), binding.backdropRootSignature.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC backdropDescription{};
  backdropDescription.pRootSignature = binding.backdropRootSignature.get();
  backdropDescription.VS = {.pShaderBytecode = g_pSkyVS, .BytecodeLength = sizeof(g_pSkyVS)};
  backdropDescription.PS = {.pShaderBytecode = g_pSkyPS, .BytecodeLength = sizeof(g_pSkyPS)};
  backdropDescription.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
  backdropDescription.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
  backdropDescription.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  backdropDescription.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  backdropDescription.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  backdropDescription.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  backdropDescription.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
  backdropDescription.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  backdropDescription.SampleMask = UINT_MAX;
  backdropDescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  backdropDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  backdropDescription.RasterizerState.DepthClipEnable = TRUE;
  backdropDescription.RasterizerState.MultisampleEnable = (SceneTarget::SAMPLE_COUNT > 1) ? TRUE : FALSE;

  // The same arrangement `PointSprites` uses and for the same reasons: tested so it shades no pixel
  // the fleet covers, `LESS_EQUAL` because the target clears to exactly the value this emits, and no
  // write because nothing is drawn after the sky.
  backdropDescription.DepthStencilState.DepthEnable = TRUE;
  backdropDescription.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  backdropDescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  backdropDescription.DepthStencilState.StencilEnable = FALSE;

  backdropDescription.InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0};
  backdropDescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  backdropDescription.NumRenderTargets = 1;
  backdropDescription.RTVFormats[0] = static_cast<DXGI_FORMAT>(_sceneTarget.ColorFormatCode());
  backdropDescription.DSVFormat = static_cast<DXGI_FORMAT>(_sceneTarget.DepthFormatCode());
  backdropDescription.SampleDesc = {.Count = SceneTarget::SAMPLE_COUNT, .Quality = 0};

  binding.lastHresult = device->CreateGraphicsPipelineState(&backdropDescription, winrt::guid_of<ID3D12PipelineState>(),
                                                            binding.backdropPipelineState.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  binding.ready = true;
  return true;
}

void CubemapBake::Destroy() noexcept
{
  CubemapBakeBinding& binding = *m_binding;
  binding.backdropPipelineState = nullptr;
  binding.backdropRootSignature = nullptr;
  binding.bakePipelineState = nullptr;
  binding.bakeRootSignature = nullptr;
  binding.samplerHeap = nullptr;
  binding.shaderResourceHeap = nullptr;
  binding.renderTargetHeap = nullptr;
  binding.cubemap = nullptr;
  binding.renderTargetStride = 0;
  binding.ready = false;
  binding.baked = false;
}

bool CubemapBake::IsReady() const noexcept
{
  return m_binding->ready;
}

bool CubemapBake::IsBaked() const noexcept
{
  return m_binding->baked;
}

std::int32_t CubemapBake::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

bool CubemapBake::Bake(const GraphicsDevice& _device, const float (&_galaxy)[GALAXY_CONSTANT_COUNT]) noexcept
{
  CubemapBakeBinding& binding = *m_binding;
  if (!binding.ready)
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  D3D12_RESOURCE_BARRIER toRenderTarget{};
  toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  toRenderTarget.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  toRenderTarget.Transition.pResource = binding.cubemap.get();
  toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  toRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  commandList->ResourceBarrier(1, &toRenderTarget);

  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0f,
                                .TopLeftY = 0.0f,
                                .Width = static_cast<float>(FACE_PIXELS),
                                .Height = static_cast<float>(FACE_PIXELS),
                                .MinDepth = 0.0f,
                                .MaxDepth = 1.0f};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = static_cast<LONG>(FACE_PIXELS), .bottom = static_cast<LONG>(FACE_PIXELS)};

  commandList->SetGraphicsRootSignature(binding.bakeRootSignature.get());
  commandList->SetPipelineState(binding.bakePipelineState.get());
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->SetGraphicsRoot32BitConstants(GALAXY_PARAMETER, GALAXY_CONSTANT_COUNT, _galaxy, 0);

  // **SIX FACES IN ONE PASS.** The galaxy's constants are set once above and only the face basis
  // changes between draws, which is what makes this one pass rather than six.
  D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle = binding.renderTargetHeap->GetCPUDescriptorHandleForHeapStart();
  for (std::uint32_t face = 0; face < FACE_COUNT; ++face)
  {
    // Three `float4`, so each axis is padded to four floats -- the shader declares them that way and
    // a tightly packed nine would silently read the next face's numbers as its own.
    std::array<float, FACE_CONSTANT_COUNT> faceConstants{};
    for (std::uint32_t axis = 0; axis < 3; ++axis)
    {
      faceConstants[axis] = FACE_BASES[face].right[axis];
      faceConstants[4 + axis] = FACE_BASES[face].up[axis];
      faceConstants[8 + axis] = FACE_BASES[face].forward[axis];
    }

    commandList->OMSetRenderTargets(1, &renderTargetHandle, FALSE, nullptr);
    commandList->SetGraphicsRoot32BitConstants(FACE_PARAMETER, FACE_CONSTANT_COUNT, faceConstants.data(), 0);
    commandList->DrawInstanced(3, 1, 0, 0);

    renderTargetHandle.ptr += binding.renderTargetStride;
  }

  D3D12_RESOURCE_BARRIER toShaderResource{};
  toShaderResource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  toShaderResource.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  toShaderResource.Transition.pResource = binding.cubemap.get();
  toShaderResource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  toShaderResource.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  toShaderResource.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  commandList->ResourceBarrier(1, &toShaderResource);

  binding.baked = true;
  return true;
}

bool CubemapBake::DrawBackdrop(const GraphicsDevice& _device, const SceneTarget& _sceneTarget,
                               const float (&_ray)[RAY_CONSTANT_COUNT]) noexcept
{
  CubemapBakeBinding& binding = *m_binding;
  if (!binding.ready || !binding.baked || !_sceneTarget.IsReady())
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0f,
                                .TopLeftY = 0.0f,
                                .Width = static_cast<float>(_sceneTarget.WidthPixels()),
                                .Height = static_cast<float>(_sceneTarget.HeightPixels()),
                                .MinDepth = 0.0f,
                                .MaxDepth = 1.0f};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = _sceneTarget.WidthPixels(), .bottom = _sceneTarget.HeightPixels()};

  std::array<ID3D12DescriptorHeap*, 2> heaps{binding.shaderResourceHeap.get(), binding.samplerHeap.get()};
  commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());

  commandList->SetGraphicsRootSignature(binding.backdropRootSignature.get());
  commandList->SetGraphicsRoot32BitConstants(RAY_PARAMETER, RAY_CONSTANT_COUNT, _ray, 0);
  commandList->SetGraphicsRootDescriptorTable(CUBEMAP_PARAMETER, binding.shaderResourceHeap->GetGPUDescriptorHandleForHeapStart());
  commandList->SetGraphicsRootDescriptorTable(SAMPLER_PARAMETER, binding.samplerHeap->GetGPUDescriptorHandleForHeapStart());
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetPipelineState(binding.backdropPipelineState.get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->DrawInstanced(3, 1, 0, 0);
  return true;
}

} // namespace Neuron
