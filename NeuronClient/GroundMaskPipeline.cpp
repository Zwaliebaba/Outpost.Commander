#include "pch.h"
#include "GroundMaskPipeline.h"

#include "CompiledShader/GroundMaskPS.h"
#include "CompiledShader/GroundMaskVS.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
// The root signature's parameters: the frame's constants at b0, as root constants, and the shades' texture at t0, as a
// table of one in the renderer's shader-visible heap, sampled smoothly at s0.
constexpr UINT FRAME_PARAMETER = 0;
constexpr UINT SHADES_PARAMETER = 1;
// One slot of the upload buffer: the whole texture, its rows TEXTURE_SIDE bytes apart, which is the pitch a copy needs.
constexpr UINT64 UPLOAD_SLOT_BYTES = UINT64{Neuron::GroundMaskPipeline::TEXTURE_SIDE} * Neuron::GroundMaskPipeline::TEXTURE_SIDE;
static_assert(Neuron::GroundMaskPipeline::TEXTURE_SIDE % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0,
              "A row of the texture is a copy's pitch.");
static_assert(UPLOAD_SLOT_BYTES % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT == 0, "Each slot starts where a copy may.");
constexpr UINT FRAME_CONSTANT_COUNT = sizeof(Neuron::GroundMaskPipeline::FrameConstants) / sizeof(UINT);
// The square is two triangles, its corners made by the vertex shader from SV_VertexID.
constexpr UINT VERTICES_PER_SQUARE = 6;

winrt::com_ptr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* _device)
{
  // The texture is copied into earlier in the same command list, so its data is only static while the table is set.
  const CD3DX12_DESCRIPTOR_RANGE1 shadesRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
                                              D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
  std::array<CD3DX12_ROOT_PARAMETER1, 2> parameters{};
  parameters[FRAME_PARAMETER].InitAsConstants(FRAME_CONSTANT_COUNT, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
  parameters[SHADES_PARAMETER].InitAsDescriptorTable(1, &shadesRange, D3D12_SHADER_VISIBILITY_PIXEL);
  // The four nearest texels blended, as the cells' centers are; the shader keeps its coordinates inside the grid.
  const CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

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
    throw winrt::hresult_error(result, winrt::to_hstring(std::format("The ground mask root signature is invalid: {}", message)));
  }

  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(
    _device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(rootSignature)));
  return rootSignature;
}
} // namespace

Neuron::GroundMaskPipeline::GroundMaskPipeline(Renderer& _renderer)
{
  ID3D12Device* device = _renderer.Device();
  m_rootSignature = CreateRootSignature(device);

  // Black, as opaque as the shade: the scene shows through what is left.
  CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);
  blend.RenderTarget[0].BlendEnable = TRUE;
  blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
  blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
  CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
  rasterizer.CullMode = D3D12_CULL_MODE_NONE;
  // Over everything drawn so far, whatever its depth, and writing none.
  CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
  depthStencil.DepthEnable = FALSE;
  depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

  // Every member with an enum that has no zero value is set here, so none is ever left at an invalid zero.
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC description{
    .pRootSignature = m_rootSignature.get(),
    .VS = CD3DX12_SHADER_BYTECODE(g_GroundMaskVS, sizeof(g_GroundMaskVS)),
    .PS = CD3DX12_SHADER_BYTECODE(g_GroundMaskPS, sizeof(g_GroundMaskPS)),
    .BlendState = blend,
    .SampleMask = UINT_MAX,
    .RasterizerState = rasterizer,
    .DepthStencilState = depthStencil,
    .InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0},
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = {Renderer::RENDER_TARGET_FORMAT},
    .DSVFormat = Renderer::DEPTH_FORMAT,
    .SampleDesc = {.Count = Renderer::SAMPLE_COUNT, .Quality = 0},
  };
  winrt::check_hresult(device->CreateGraphicsPipelineState(&description, IID_GRAPHICS_PPV_ARGS(m_pipelineState)));

  const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
  const CD3DX12_RESOURCE_DESC shadesDescription = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8_UNORM, TEXTURE_SIDE, TEXTURE_SIDE, 1, 1);
  winrt::check_hresult(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &shadesDescription,
                                                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                                                       IID_GRAPHICS_PPV_ARGS(m_shades)));
  const UINT slot = _renderer.TakeShaderView();
  const D3D12_SHADER_RESOURCE_VIEW_DESC view{
    .Format = DXGI_FORMAT_R8_UNORM,
    .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0f}};
  device->CreateShaderResourceView(m_shades.get(), &view, _renderer.ShaderViewCpu(slot));
  m_shadesView = _renderer.ShaderViewGpu(slot);

  const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
  const CD3DX12_RESOURCE_DESC uploadDescription = CD3DX12_RESOURCE_DESC::Buffer(UPLOAD_SLOT_BYTES * Renderer::FRAME_COUNT);
  winrt::check_hresult(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_GRAPHICS_PPV_ARGS(m_upload)));
  const D3D12_RANGE nothingRead{.Begin = 0, .End = 0};
  void* mapped = nullptr;
  winrt::check_hresult(m_upload->Map(0, &nothingRead, &mapped));
  m_mappedUpload = static_cast<std::uint8_t*>(mapped);
}

void Neuron::GroundMaskPipeline::SetShades(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, std::span<const float> _shades,
                                           UINT _cellsPerSide, std::span<const std::uint8_t> _changedRows)
{
  if (_cellsPerSide == 0 || _cellsPerSide > TEXTURE_SIDE || _shades.size() != size_t{_cellsPerSide} * _cellsPerSide ||
      (!_changedRows.empty() && _changedRows.size() != _cellsPerSide))
    return;
  // A byte a cell, into this frame's slot, which the GPU is done with: the renderer waited for this frame index's previous
  // frame before handing out the command list. The row and the column past the grid repeat its last, where there is room.
  // A texture of another grid's size is written whole.
  const UINT side = std::min(_cellsPerSide + 1, TEXTURE_SIDE);
  const UINT64 offset = UPLOAD_SLOT_BYTES * _frameIndex;
  std::uint8_t* texels = m_mappedUpload + offset;
  const bool everyRow = _changedRows.empty() || m_cellsPerSide != _cellsPerSide;
  const auto changed = [&](UINT _z) { return everyRow || _changedRows[std::min(_z, _cellsPerSide - 1)] != 0; };
  const D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{
    .Offset = offset, .Footprint = {.Format = DXGI_FORMAT_R8_UNORM, .Width = side, .Height = side, .Depth = 1, .RowPitch = TEXTURE_SIDE}};
  const CD3DX12_TEXTURE_COPY_LOCATION destination(m_shades.get(), 0);
  const CD3DX12_TEXTURE_COPY_LOCATION source(m_upload.get(), footprint);
  bool copying = false;
  // Each run of changed rows is written and copied as one region.
  for (UINT z = 0; z < side;)
  {
    if (!changed(z))
    {
      ++z;
      continue;
    }
    const UINT first = z;
    for (; z < side && changed(z); ++z)
    {
      const float* row = _shades.data() + (size_t{std::min(z, _cellsPerSide - 1)} * _cellsPerSide);
      std::uint8_t* out = texels + (size_t{z} * TEXTURE_SIDE);
      for (UINT x = 0; x < side; ++x)
        out[x] = static_cast<std::uint8_t>(std::lround(std::clamp(row[std::min(x, _cellsPerSide - 1)], 0.0f, 1.0f) * 255.0f));
    }
    if (!copying)
    {
      const auto toCopy =
        CD3DX12_RESOURCE_BARRIER::Transition(m_shades.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
      _commandList->ResourceBarrier(1, &toCopy);
      copying = true;
    }
    const D3D12_BOX rows{.left = 0, .top = first, .front = 0, .right = side, .bottom = z, .back = 1};
    _commandList->CopyTextureRegion(&destination, 0, first, 0, &source, &rows);
  }
  m_cellsPerSide = _cellsPerSide;
  if (!copying)
    return;
  const auto toShader =
    CD3DX12_RESOURCE_BARRIER::Transition(m_shades.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  _commandList->ResourceBarrier(1, &toShader);
}

void Neuron::GroundMaskPipeline::Draw(ID3D12GraphicsCommandList* _commandList, const FrameConstants& _constants) const
{
  if (m_cellsPerSide == 0 || _constants.cellsPerSide != m_cellsPerSide)
    return;
  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->SetGraphicsRoot32BitConstants(FRAME_PARAMETER, FRAME_CONSTANT_COUNT, &_constants, 0);
  _commandList->SetGraphicsRootDescriptorTable(SHADES_PARAMETER, m_shadesView);
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _commandList->DrawInstanced(VERTICES_PER_SQUARE, 1, 0, 0);
}
