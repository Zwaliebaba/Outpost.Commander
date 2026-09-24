#include "pch.h"

#include "MeshPass.h"

// ADR-012: `dxc` compiles each stage into a header at build time and the header is checked in, so
// there is no file to read at device creation and a missing shader is a compile error.
#include "CompiledShader/ShipPS.h"
#include "CompiledShader/ShipVS.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>
#include <climits>

// `D3D12SerializeRootSignature` is a free function and therefore an import, as it is in
// `WorldPass.cpp`. The link dependency travels with the code that needs it.
#pragma comment(lib, "d3d12.lib")

namespace Neuron
{

struct MeshPassBinding
{
  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::com_ptr<ID3D12PipelineState> pipelineState;

  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
inline constexpr std::uint32_t TRANSFORM_PARAMETER = 0;
inline constexpr std::uint32_t LOOK_PARAMETER = 1;

/// The two vertex-buffer slots. Slot zero steps per vertex and slot one per instance, which is the
/// whole of what makes this an instanced draw rather than a loop.
inline constexpr std::uint32_t VERTEX_SLOT = 0;
inline constexpr std::uint32_t INSTANCE_SLOT = 1;

[[nodiscard]] bool OpenCommandList(const GraphicsDevice& _device, winrt::com_ptr<ID3D12GraphicsCommandList>& _outList) noexcept
{
  return (_device.CommandListUnknown() != nullptr) &&
         SUCCEEDED(_device.CommandListUnknown()->QueryInterface(winrt::guid_of<ID3D12GraphicsCommandList>(), _outList.put_void()));
}
} // namespace

MeshPass::MeshPass() noexcept
  : m_binding(std::make_shared<MeshPassBinding>())
{
}

MeshPass::~MeshPass() noexcept = default;

bool MeshPass::Create(const GraphicsDevice& _device, const SceneTarget& _sceneTarget) noexcept
{
  MeshPassBinding& binding = *m_binding;
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

  std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
  parameters[TRANSFORM_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[TRANSFORM_PARAMETER].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = TRANSFORM_CONSTANT_COUNT};
  parameters[TRANSFORM_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  parameters[LOOK_PARAMETER].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[LOOK_PARAMETER].Constants = {.ShaderRegister = 1, .RegisterSpace = 0, .Num32BitValues = LOOK_CONSTANT_COUNT};
  parameters[LOOK_PARAMETER].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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

  // **SLOT ZERO STEPS PER VERTEX AND SLOT ONE PER INSTANCE.** The `InstanceDataStepRate` of one on
  // the second pair is the whole of what makes this an instanced draw; a zero there would draw every
  // ship at the first instance's position, which looks like the instance buffer not being filled.
  const std::array<D3D12_INPUT_ELEMENT_DESC, 5> elements{
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "POSITION",
                             .SemanticIndex = 0,
                             .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                             .InputSlot = VERTEX_SLOT,
                             .AlignedByteOffset = 0,
                             .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                             .InstanceDataStepRate = 0},
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "NORMAL",
                             .SemanticIndex = 0,
                             .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                             .InputSlot = VERTEX_SLOT,
                             .AlignedByteOffset = 12,
                             .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                             .InstanceDataStepRate = 0},
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "TEXCOORD",
                             .SemanticIndex = 0,
                             .Format = DXGI_FORMAT_R32G32_FLOAT,
                             .InputSlot = VERTEX_SLOT,
                             .AlignedByteOffset = 24,
                             .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                             .InstanceDataStepRate = 0},
    D3D12_INPUT_ELEMENT_DESC{.SemanticName = "TEXCOORD",
                             .SemanticIndex = 1,
                             .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                             .InputSlot = INSTANCE_SLOT,
                             .AlignedByteOffset = 0,
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
  pipelineDescription.VS = {.pShaderBytecode = g_pShipVS, .BytecodeLength = sizeof(g_pShipVS)};
  pipelineDescription.PS = {.pShaderBytecode = g_pShipPS, .BytecodeLength = sizeof(g_pShipPS)};

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

  // **CULLING IS ON HERE, WHERE `WorldPass` HAS IT OFF, AND THE DIFFERENCE IS THE POINT.** That pass
  // generates its shape from a vertex identifier, so its winding is whatever the arithmetic produced
  // and culling would hide a shape rather than a bug. This one draws authored geometry through two
  // reflections -- `GameClient/HullMesh`'s left-handed handoff into a right-handed world, and the
  // view's right-handed world into Direct3D's left-handed view -- which cancel, so the handoff's
  // clockwise front faces arrive clockwise. Culling is what PROVES the two still agree: a hull drawn
  // inside out here is one of them having been undone on its own.
  pipelineDescription.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
  pipelineDescription.RasterizerState.FrontCounterClockwise = FALSE;
  pipelineDescription.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
  pipelineDescription.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
  pipelineDescription.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
  pipelineDescription.RasterizerState.DepthClipEnable = TRUE;
  pipelineDescription.RasterizerState.MultisampleEnable = (SceneTarget::SAMPLE_COUNT > 1) ? TRUE : FALSE;
  pipelineDescription.RasterizerState.AntialiasedLineEnable = FALSE;
  pipelineDescription.RasterizerState.ForcedSampleCount = 0;
  pipelineDescription.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  pipelineDescription.DepthStencilState.DepthEnable = TRUE;
  pipelineDescription.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  pipelineDescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
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

void MeshPass::Destroy() noexcept
{
  MeshPassBinding& binding = *m_binding;
  binding.pipelineState = nullptr;
  binding.rootSignature = nullptr;
  binding.ready = false;
}

bool MeshPass::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t MeshPass::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

bool MeshPass::Begin(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, const float (&_viewProjection)[16],
                     const Look& _look) noexcept
{
  MeshPassBinding& binding = *m_binding;
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

  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0f,
                                .TopLeftY = 0.0f,
                                .Width = static_cast<float>(_sceneTarget.WidthPixels()),
                                .Height = static_cast<float>(_sceneTarget.HeightPixels()),
                                .MinDepth = 0.0f,
                                .MaxDepth = 1.0f};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = _sceneTarget.WidthPixels(), .bottom = _sceneTarget.HeightPixels()};

  commandList->SetGraphicsRootSignature(binding.rootSignature.get());
  commandList->SetGraphicsRoot32BitConstants(TRANSFORM_PARAMETER, TRANSFORM_CONSTANT_COUNT, _viewProjection, 0);
  commandList->SetGraphicsRoot32BitConstants(LOOK_PARAMETER, LOOK_CONSTANT_COUNT, &_look, 0);
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetPipelineState(binding.pipelineState.get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  return true;
}

bool MeshPass::Draw(const GraphicsDevice& _device, const MeshBuffer& _mesh, std::uint64_t _instanceAddress, std::uint32_t _instanceBytes,
                    std::uint32_t _instanceCount) noexcept
{
  MeshPassBinding& binding = *m_binding;
  if (!binding.ready || !_mesh.IsReady() || (_instanceCount == 0) || (_instanceAddress == 0))
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  const std::array<D3D12_VERTEX_BUFFER_VIEW, 2> vertexViews{
    D3D12_VERTEX_BUFFER_VIEW{
      .BufferLocation = _mesh.VertexBufferAddress(), .SizeInBytes = _mesh.VertexBufferBytes(), .StrideInBytes = _mesh.VertexStride()},
    D3D12_VERTEX_BUFFER_VIEW{
      .BufferLocation = _instanceAddress, .SizeInBytes = _instanceBytes, .StrideInBytes = static_cast<UINT>(sizeof(MeshInstance))}};

  const D3D12_INDEX_BUFFER_VIEW indexView{
    .BufferLocation = _mesh.IndexBufferAddress(), .SizeInBytes = _mesh.IndexBufferBytes(), .Format = DXGI_FORMAT_R16_UINT};

  commandList->IASetVertexBuffers(VERTEX_SLOT, static_cast<UINT>(vertexViews.size()), vertexViews.data());
  commandList->IASetIndexBuffer(&indexView);
  commandList->DrawIndexedInstanced(_mesh.IndexCount(), _instanceCount, 0, 0, 0);
  return true;
}

} // namespace Neuron
