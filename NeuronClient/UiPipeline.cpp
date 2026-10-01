#include "pch.h"
#include "UiPipeline.h"

#include "CompiledShader/UiPS.h"
#include "CompiledShader/UiVS.h"

#include <cmath>
#include <cstring>

namespace
{
// The root signature's parameters, in order: the screen's size at b0, the atlas at t0.
constexpr UINT SCREEN_PARAMETER = 0;
constexpr UINT ATLAS_PARAMETER = 1;
constexpr UINT SCREEN_CONSTANT_COUNT = 4;

constexpr UINT VERTICES_PER_QUAD = 4;
constexpr UINT INDICES_PER_QUAD = 6;

winrt::com_ptr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* _device)
{
  const CD3DX12_DESCRIPTOR_RANGE1 atlasRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
  std::array<CD3DX12_ROOT_PARAMETER1, 2> parameters{};
  parameters[SCREEN_PARAMETER].InitAsConstants(SCREEN_CONSTANT_COUNT, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[ATLAS_PARAMETER].InitAsDescriptorTable(1, &atlasRange, D3D12_SHADER_VISIBILITY_PIXEL);
  // Texels map one to one onto pixels, so filtering only matters at a glyph's edge; clamping keeps it inside the atlas.
  const CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC description;
  description.Init_1_1(static_cast<UINT>(parameters.size()), parameters.data(), 1, &sampler,
                       D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
    throw winrt::hresult_error(result, winrt::to_hstring(std::format("The interface root signature is invalid: {}", message)));
  }

  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(
    _device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(rootSignature)));
  return rootSignature;
}
} // namespace

Neuron::UiPipeline::UiPipeline(Renderer& _renderer, std::wstring_view _fontFamily, float _fontPixels)
  : m_renderer(_renderer),
    m_fontFamily(_fontFamily)
{
  ID3D12Device* device = _renderer.Device();
  m_rootSignature = CreateRootSignature(device);

  // Matches Vertex.
  const std::array<D3D12_INPUT_ELEMENT_DESC, 3> inputLayout{{
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, texel), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  }};

  // Straight alpha over what is already drawn; the interface is flat, so nothing is culled and depth is ignored.
  CD3DX12_BLEND_DESC blend(D3D12_DEFAULT);
  blend.RenderTarget[0].BlendEnable = TRUE;
  blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  CD3DX12_RASTERIZER_DESC rasterizer(D3D12_DEFAULT);
  rasterizer.CullMode = D3D12_CULL_MODE_NONE;
  CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
  depthStencil.DepthEnable = FALSE;
  depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

  // Every member with an enum that has no zero value is set here, so none is ever left at an invalid zero.
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC description{
    .pRootSignature = m_rootSignature.get(),
    .VS = CD3DX12_SHADER_BYTECODE(g_UiVS, sizeof(g_UiVS)),
    .PS = CD3DX12_SHADER_BYTECODE(g_UiPS, sizeof(g_UiPS)),
    .BlendState = blend,
    .SampleMask = UINT_MAX,
    .RasterizerState = rasterizer,
    .DepthStencilState = depthStencil,
    .InputLayout = {.pInputElementDescs = inputLayout.data(), .NumElements = static_cast<UINT>(inputLayout.size())},
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = {Renderer::RENDER_TARGET_FORMAT},
    .DSVFormat = Renderer::DEPTH_FORMAT,
    .SampleDesc = {.Count = 1, .Quality = 0},
  };
  winrt::check_hresult(device->CreateGraphicsPipelineState(&description, IID_GRAPHICS_PPV_ARGS(m_pipelineState)));

  const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .NumDescriptors = 1, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, .NodeMask = 0};
  winrt::check_hresult(device->CreateDescriptorHeap(&heapDescription, IID_GRAPHICS_PPV_ARGS(m_descriptorHeap)));

  // Every quad is two triangles over its four corners: top-left, top-right, bottom-right, bottom-left.
  std::vector<std::uint16_t> indices;
  indices.reserve(std::size_t{MAX_QUADS} * INDICES_PER_QUAD);
  for (UINT quad = 0; quad < MAX_QUADS; ++quad)
  {
    const auto first = static_cast<std::uint16_t>(quad * VERTICES_PER_QUAD);
    for (const std::uint16_t corner : {0, 1, 2, 0, 2, 3})
      indices.push_back(static_cast<std::uint16_t>(first + corner));
  }
  const auto indexBytes = std::as_bytes(std::span(indices));
  m_indexBuffer = _renderer.CreateStaticBuffer(indexBytes);
  m_indexBufferView = {.BufferLocation = m_indexBuffer->GetGPUVirtualAddress(),
                       .SizeInBytes = static_cast<UINT>(indexBytes.size()),
                       .Format = DXGI_FORMAT_R16_UINT};

  const CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
  const CD3DX12_RESOURCE_DESC vertexDescription =
    CD3DX12_RESOURCE_DESC::Buffer(UINT64{sizeof(Vertex)} * VERTICES_PER_QUAD * MAX_QUADS * Renderer::FRAME_COUNT);
  winrt::check_hresult(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vertexDescription,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_GRAPHICS_PPV_ARGS(m_vertices)));
  const D3D12_RANGE nothingRead{.Begin = 0, .End = 0};
  void* mapped = nullptr;
  winrt::check_hresult(m_vertices->Map(0, &nothingRead, &mapped));
  m_mappedVertices = static_cast<Vertex*>(mapped);

  Rasterize(_fontPixels);
}

void Neuron::UiPipeline::Rasterize(float _fontPixels)
{
  m_fontPixels = std::round(_fontPixels);
  m_atlas = RasterizeGlyphs(m_fontFamily, m_fontPixels);
  // Uploading waits for every frame in flight, so the old texture is no longer read when it is replaced.
  m_atlasTexture =
    m_renderer.CreateStaticTexture(m_atlas.width, m_atlas.height, DXGI_FORMAT_R8_UNORM, std::as_bytes(std::span(m_atlas.coverage)));
  const D3D12_SHADER_RESOURCE_VIEW_DESC view{
    .Format = DXGI_FORMAT_R8_UNORM,
    .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1, .PlaneSlice = 0, .ResourceMinLODClamp = 0.0f}};
  m_renderer.Device()->CreateShaderResourceView(m_atlasTexture.get(), &view, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void Neuron::UiPipeline::Begin(UINT _widthPixels, UINT _heightPixels, float _fontPixels)
{
  if (std::abs(std::round(_fontPixels) - m_fontPixels) >= 1.0f)
    Rasterize(_fontPixels);
  m_widthPixels = static_cast<float>(_widthPixels);
  m_heightPixels = static_cast<float>(_heightPixels);
  m_frameVertices.clear();
}

void Neuron::UiPipeline::AddQuad(float _left, float _top, float _right, float _bottom, float _u0, float _v0, float _u1, float _v1,
                                 const DirectX::XMFLOAT4& _color)
{
  if (m_frameVertices.size() >= std::size_t{MAX_QUADS} * VERTICES_PER_QUAD)
    return;
  m_frameVertices.push_back({{_left, _top}, {_u0, _v0}, _color});
  m_frameVertices.push_back({{_right, _top}, {_u1, _v0}, _color});
  m_frameVertices.push_back({{_right, _bottom}, {_u1, _v1}, _color});
  m_frameVertices.push_back({{_left, _bottom}, {_u0, _v1}, _color});
}

void Neuron::UiPipeline::FillRect(float _left, float _top, float _width, float _height, const DirectX::XMFLOAT4& _color)
{
  // Every corner samples the solid block's center, which is full coverage.
  const float u = (static_cast<float>(m_atlas.solidX) + 0.5f) / static_cast<float>(m_atlas.width);
  const float v = (static_cast<float>(m_atlas.solidY) + 0.5f) / static_cast<float>(m_atlas.height);
  AddQuad(_left, _top, _left + _width, _top + _height, u, v, u, v, _color);
}

void Neuron::UiPipeline::DrawText(std::string_view _text, float _left, float _top, const DirectX::XMFLOAT4& _color)
{
  // Whole pixels, so that texels land on pixels and the text stays as DirectWrite drew it.
  float pen = std::round(_left);
  const float baseline = std::round(_top) + m_atlas.ascent;
  const auto width = static_cast<float>(m_atlas.width);
  const auto height = static_cast<float>(m_atlas.height);
  for (const char character : _text)
  {
    const GlyphAtlas::Glyph& glyph = m_atlas.For(character);
    if (glyph.width > 0)
    {
      const float left = pen + static_cast<float>(glyph.offsetX);
      const float top = baseline + static_cast<float>(glyph.offsetY);
      AddQuad(left, top, left + static_cast<float>(glyph.width), top + static_cast<float>(glyph.height),
              static_cast<float>(glyph.atlasX) / width, static_cast<float>(glyph.atlasY) / height,
              static_cast<float>(glyph.atlasX + glyph.width) / width, static_cast<float>(glyph.atlasY + glyph.height) / height, _color);
    }
    pen += glyph.advance;
  }
}

void Neuron::UiPipeline::End(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex)
{
  if (m_frameVertices.empty())
    return;
  // The renderer waited for this frame index's previous frame before handing out the command list, so the GPU is done
  // with this slot.
  const std::size_t slot = std::size_t{_frameIndex} * MAX_QUADS * VERTICES_PER_QUAD;
  std::memcpy(m_mappedVertices + slot, m_frameVertices.data(), m_frameVertices.size() * sizeof(Vertex));

  const D3D12_VERTEX_BUFFER_VIEW vertexView{
    .BufferLocation = m_vertices->GetGPUVirtualAddress() + (slot * sizeof(Vertex)),
    .SizeInBytes = static_cast<UINT>(m_frameVertices.size() * sizeof(Vertex)),
    .StrideInBytes = sizeof(Vertex),
  };
  const std::array<float, SCREEN_CONSTANT_COUNT> screen{m_widthPixels, m_heightPixels, 0.0f, 0.0f};
  ID3D12DescriptorHeap* heaps[] = {m_descriptorHeap.get()};

  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->SetDescriptorHeaps(1, heaps);
  _commandList->SetGraphicsRoot32BitConstants(SCREEN_PARAMETER, SCREEN_CONSTANT_COUNT, screen.data(), 0);
  _commandList->SetGraphicsRootDescriptorTable(ATLAS_PARAMETER, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _commandList->IASetVertexBuffers(0, 1, &vertexView);
  _commandList->IASetIndexBuffer(&m_indexBufferView);
  const auto quads = static_cast<UINT>(m_frameVertices.size() / VERTICES_PER_QUAD);
  _commandList->DrawIndexedInstanced(quads * INDICES_PER_QUAD, 1, 0, 0, 0);
}
