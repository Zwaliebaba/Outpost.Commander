#include "pch.h"

#include "CursorPass.h"

#include "Log.h"
#include "SceneTarget.h"

// Written by the shader compiler on every build (AGENTS.md §2); nothing else includes them.
#include "CompiledShaders/CursorPS.h"
#include "CompiledShaders/CursorVS.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <iterator>
#include <string>

namespace Neuron
{

namespace
{

constexpr DXGI_FORMAT RING_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr std::uint32_t RING_BYTES_PER_PIXEL = 4;

/// Species's own size for the highlight disc and the rule that keeps it the same size on screen
/// without making it a billboard: 30 world units, times the square root of the camera's distance,
/// over 40 (GameCursor.cpp's MouseCursor::Render3D). About 15 units across at 400 away and 37 at
/// 2,500, which is Design/Interface.md §4's table.
constexpr float RING_SIZE_WORLD_UNITS = 30.0f;
constexpr float RING_DISTANCE_DIVISOR = 40.0f;

/// The bias that keeps a quad lying ON the ground from fighting the ground for the depth test.
///
/// THE SIGN IS THE REVERSED DEPTH'S (ADR-005): the near plane is 1 and the far plane is 0, so
/// "toward the camera" is a LARGER depth value and the bias is positive. Under the ordinary
/// mapping it would be negative, which is what every example on the internet says, and is what
/// would make the ring vanish into the hillside here. Species nudges its near plane out by five
/// percent for this pass instead, which is a GL trick with no clean D3D12 equivalent.
constexpr INT DEPTH_BIAS = 500;
constexpr FLOAT SLOPE_SCALED_DEPTH_BIAS = 4.0f;

[[nodiscard]] winrt::com_ptr<ID3D12Resource> UploadBuffer(ID3D12Device* _device, std::size_t _size, const wchar_t* _name)
{
  winrt::com_ptr<ID3D12Resource> buffer;
  const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
  const CD3DX12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::Buffer(std::max<std::size_t>(_size, 16));
  winrt::check_hresult(_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                        nullptr, IID_PPV_ARGS(buffer.put())));
  winrt::check_hresult(buffer->SetName(_name));
  return buffer;
}

/// The texture, its upload buffer and its view, with the decoded pixels copied in at the pitch the
/// device asks for - which is 256-byte aligned and is not the image's own row length in general.
void PrepareRing(ID3D12Device* _device, std::span<const std::uint8_t> _pixels, std::uint32_t _width, std::uint32_t _height,
                 const wchar_t* _name, winrt::com_ptr<ID3D12Resource>& _outTexture, winrt::com_ptr<ID3D12Resource>& _outUpload,
                 D3D12_PLACED_SUBRESOURCE_FOOTPRINT& _outFootprint, D3D12_CPU_DESCRIPTOR_HANDLE _view)
{
  const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
  const CD3DX12_RESOURCE_DESC description = CD3DX12_RESOURCE_DESC::Tex2D(RING_FORMAT, _width, _height, 1, 1, 1, 0);
  winrt::check_hresult(_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, IID_PPV_ARGS(_outTexture.put())));
  winrt::check_hresult(_outTexture->SetName(_name));

  std::uint64_t totalBytes = 0;
  _device->GetCopyableFootprints(&description, 0, 1, 0, &_outFootprint, nullptr, nullptr, &totalBytes);
  _outUpload = UploadBuffer(_device, static_cast<std::size_t>(totalBytes), _name);

  void* mapped = nullptr;
  const D3D12_RANGE nothing{0, 0};
  winrt::check_hresult(_outUpload->Map(0, &nothing, &mapped));
  auto* destination = static_cast<std::uint8_t*>(mapped);
  const std::size_t sourcePitch = static_cast<std::size_t>(_width) * RING_BYTES_PER_PIXEL;
  for (std::uint32_t row = 0; row < _height; ++row)
  {
    std::memcpy(destination + _outFootprint.Offset + static_cast<std::size_t>(row) * _outFootprint.Footprint.RowPitch,
                _pixels.data() + static_cast<std::size_t>(row) * sourcePitch, sourcePitch);
  }
  _outUpload->Unmap(0, nullptr);

  D3D12_SHADER_RESOURCE_VIEW_DESC view{};
  view.Format = RING_FORMAT;
  view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  view.Texture2D.MipLevels = 1;
  _device->CreateShaderResourceView(_outTexture.get(), &view, _view);
}

} // namespace

CursorPass::CursorPass(GraphicsDevice& _device, const Texture& _sharp, const Texture& _blurred, std::uint32_t _sceneSampleCount)
  : m_device(&_device),
    m_views(_device.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, true)
{
  ID3D12Device* device = _device.Device();

  // b0 the terrain pass's scene constants, b1 the tint as four root constants, t0 whichever of the
  // two rings this draw is for - the TABLE chooses it, so the shader reads one texture and needs no
  // branch - and s0 a linear sampler, because the ring is scaled far past its 128 pixels.
  const CD3DX12_DESCRIPTOR_RANGE ringRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  CD3DX12_ROOT_PARAMETER parameters[3];
  parameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[1].InitAsConstants(4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[2].InitAsDescriptorTable(1, &ringRange, D3D12_SHADER_VISIBILITY_PIXEL);
  CD3DX12_STATIC_SAMPLER_DESC sampler;
  sampler.Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
               D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  const CD3DX12_ROOT_SIGNATURE_DESC description(3, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  winrt::com_ptr<ID3DBlob> serialized;
  winrt::com_ptr<ID3DBlob> errors;
  const HRESULT result = D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, serialized.put(), errors.put());
  if (FAILED(result))
  {
    if (errors)
    {
      Log::Write(LogLevel::Error, std::string("cursor: root signature: ") + static_cast<const char*>(errors->GetBufferPointer()));
    }
    winrt::throw_hresult(result);
  }
  winrt::check_hresult(
    device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.put())));

  const D3D12_INPUT_ELEMENT_DESC elements[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, x), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, u), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };

  // Every member set by hand rather than from {}, for the reason PresentPass.cpp gives.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline;
  pipeline.pRootSignature = m_rootSignature.get();
  pipeline.VS = CD3DX12_SHADER_BYTECODE(g_CursorVS, sizeof g_CursorVS);
  pipeline.PS = CD3DX12_SHADER_BYTECODE(g_CursorPS, sizeof g_CursorPS);
  pipeline.DS = D3D12_SHADER_BYTECODE{};
  pipeline.HS = D3D12_SHADER_BYTECODE{};
  pipeline.GS = D3D12_SHADER_BYTECODE{};
  pipeline.StreamOutput = D3D12_STREAM_OUTPUT_DESC{};
  pipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  pipeline.BlendState.RenderTarget[0].BlendEnable = TRUE;
  pipeline.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  pipeline.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_COLOR;
  pipeline.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
  pipeline.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
  pipeline.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  pipeline.SampleMask = UINT_MAX;
  pipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  // No culling, because the quad is oriented by the ground and a hillside can turn it away from
  // the camera; and biased toward the camera, because it is coplanar with what it lies on.
  pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeline.RasterizerState.DepthBias = DEPTH_BIAS;
  pipeline.RasterizerState.SlopeScaledDepthBias = SLOPE_SCALED_DEPTH_BIAS;
  pipeline.RasterizerState.DepthBiasClamp = 0.0f;
  pipeline.RasterizerState.MultisampleEnable = _sceneSampleCount > 1 ? TRUE : FALSE;
  pipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  // Tested and never written (§4's table): the cursor hides nothing drawn after it, and a ring
  // that wrote depth would punch a hole in the terrain for whatever came next.
  pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
  pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  pipeline.InputLayout = D3D12_INPUT_LAYOUT_DESC{elements, static_cast<UINT>(std::size(elements))};
  pipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline.NumRenderTargets = 1;
  std::fill(std::begin(pipeline.RTVFormats), std::end(pipeline.RTVFormats), DXGI_FORMAT_UNKNOWN);
  pipeline.RTVFormats[0] = SCENE_COLOR_FORMAT;
  pipeline.DSVFormat = SCENE_DEPTH_FORMAT;
  pipeline.SampleDesc = DXGI_SAMPLE_DESC{_sceneSampleCount, 0};
  pipeline.NodeMask = 0;
  pipeline.CachedPSO = D3D12_CACHED_PIPELINE_STATE{};
  pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  winrt::check_hresult(device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(m_blurredPipeline.put())));
  // The sharp pass differs in one field: it adds rather than darkening (§4's table).
  pipeline.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
  winrt::check_hresult(device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(m_sharpPipeline.put())));

  // A texture that did not load leaves a 1x1 opaque white texel, so the table is complete and the
  // shader reads something defined; the ring then draws as a white square, which is a picture that
  // says "the ring is missing" rather than a frame with no cursor at all.
  static constexpr std::uint8_t WHITE_TEXEL[RING_BYTES_PER_PIXEL] = {255, 255, 255, 255};
  const std::array<Texture, 2> sources{_blurred, _sharp}; // index 0 is drawn first
  const std::array<const wchar_t*, 2> names{L"cursor ring, blurred", L"cursor ring"};
  for (std::uint32_t index = 0; index < 2; ++index)
  {
    const bool loaded =
      sources[index].width > 0 && sources[index].height > 0 &&
      sources[index].pixels.size() >= static_cast<std::size_t>(sources[index].width) * sources[index].height * RING_BYTES_PER_PIXEL;
    PrepareRing(device, loaded ? sources[index].pixels : std::span<const std::uint8_t>(WHITE_TEXEL), loaded ? sources[index].width : 1u,
                loaded ? sources[index].height : 1u, names[index], m_textures[index], m_uploads[index], m_footprints[index],
                m_views.Cpu(m_views.Allocate()));
    if (!loaded)
    {
      Log::Write(LogLevel::Warning, std::string("cursor: the ring texture did not load; the cursor draws a white square"));
    }
  }

  for (std::uint32_t slot = 0; slot < FRAMES_IN_FLIGHT; ++slot)
  {
    m_vertices[slot] = UploadBuffer(device, VERTEX_BUFFER_BYTES, L"cursor vertices");
    void* mapped = nullptr;
    const D3D12_RANGE nothing{0, 0};
    winrt::check_hresult(m_vertices[slot]->Map(0, &nothing, &mapped));
    m_verticesMapped[slot] = static_cast<std::uint8_t*>(mapped);
  }
}

void CursorPass::UploadTextures(ID3D12GraphicsCommandList* _list)
{
  for (std::uint32_t index = 0; index < 2; ++index)
  {
    const CD3DX12_TEXTURE_COPY_LOCATION source(m_uploads[index].get(), m_footprints[index]);
    const CD3DX12_TEXTURE_COPY_LOCATION destination(m_textures[index].get(), 0);
    _list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    const CD3DX12_RESOURCE_BARRIER toRead = CD3DX12_RESOURCE_BARRIER::Transition(m_textures[index].get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                                                                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    _list->ResourceBarrier(1, &toRead);
  }
  m_uploaded = true;
}

void CursorPass::Emit(ID3D12GraphicsCommandList* _list, D3D12_GPU_VIRTUAL_ADDRESS _constants, const std::array<Vertex, 6>& _corners,
                      const std::array<float, 4>& _tint)
{
  if (!m_uploaded)
  {
    UploadTextures(_list);
  }
  if (m_used >= MAX_QUADS)
  {
    return;
  }
  const std::uint32_t slot = m_device->FrameIndex();
  const std::uint32_t first = m_used * VERTICES_PER_QUAD;
  std::memcpy(m_verticesMapped[slot] + static_cast<std::size_t>(first) * sizeof(Vertex), _corners.data(), sizeof _corners);
  ++m_used;

  D3D12_VERTEX_BUFFER_VIEW vertexView{};
  vertexView.BufferLocation = m_vertices[slot]->GetGPUVirtualAddress();
  vertexView.SizeInBytes = static_cast<UINT>(VERTEX_BUFFER_BYTES);
  vertexView.StrideInBytes = sizeof(Vertex);

  ID3D12DescriptorHeap* heaps[] = {m_views.Heap()};
  _list->SetDescriptorHeaps(1, heaps);
  _list->SetGraphicsRootSignature(m_rootSignature.get());
  _list->SetGraphicsRootConstantBufferView(0, _constants);
  _list->SetGraphicsRoot32BitConstants(1, 4, _tint.data(), 0);
  _list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _list->IASetVertexBuffers(0, 1, &vertexView);

  // THE BLURRED PASS THEN THE SHARP ONE (§4's table). The dark one is what makes a bright ring
  // readable over bright sand, and it has to be underneath or it darkens the ring instead.
  _list->SetPipelineState(m_blurredPipeline.get());
  _list->SetGraphicsRootDescriptorTable(2, m_views.Gpu(0));
  _list->DrawInstanced(VERTICES_PER_QUAD, 1, first, 0);
  _list->SetPipelineState(m_sharpPipeline.get());
  _list->SetGraphicsRootDescriptorTable(2, m_views.Gpu(1));
  _list->DrawInstanced(VERTICES_PER_QUAD, 1, first, 0);
  m_lastQuadsDrawn += 2;
}

void CursorPass::Draw(ID3D12GraphicsCommandList* _list, D3D12_GPU_VIRTUAL_ADDRESS _constants, const GroundHit& _hit, float _cameraX,
                      float _cameraY, float _cameraZ, const std::array<float, 4>& _tint, float _pulse)
{
  if (!_hit.hit)
  {
    return; // Off the landscape the cursor is HIDDEN, and the last position is not reused (§4).
  }
  // THE BASIS, AND ITS DEGENERATE CASE BRANCHED (§4's table; SpeciesLook.md §7.1). Flat ground
  // gives a normal of exactly (0, 1, 0), whose cross product with world up is the zero vector;
  // normalising that gives zero and the quad collapses to a point, which is a live fault in Species
  // and would show over most of this landscape. So the fallback is named rather than normalised.
  const float crossX = (_hit.ny * 0.0f) - (_hit.nz * 1.0f);
  const float crossY = 0.0f; // up x (0,1,0) has no y component by construction
  const float crossZ = (_hit.nx * 1.0f) - (_hit.ny * 0.0f);
  const float crossLength = std::sqrt((crossX * crossX) + (crossY * crossY) + (crossZ * crossZ));
  const float frontX = crossLength > 1e-4f ? crossX / crossLength : 0.0f;
  const float frontY = crossLength > 1e-4f ? crossY / crossLength : 0.0f;
  const float frontZ = crossLength > 1e-4f ? crossZ / crossLength : 1.0f;
  // right = normalize(front x up). Both are unit and perpendicular, so the result already is.
  const float rightX = (frontY * _hit.nz) - (frontZ * _hit.ny);
  const float rightY = (frontZ * _hit.nx) - (frontX * _hit.nz);
  const float rightZ = (frontX * _hit.ny) - (frontY * _hit.nx);

  const float toCameraX = _cameraX - _hit.x;
  const float toCameraY = _cameraY - _hit.y;
  const float toCameraZ = _cameraZ - _hit.z;
  const float distance = std::sqrt((toCameraX * toCameraX) + (toCameraY * toCameraY) + (toCameraZ * toCameraZ));
  const float size = RING_SIZE_WORLD_UNITS * std::sqrt(std::max(distance, 1.0f)) / RING_DISTANCE_DIVISOR * std::max(_pulse, 0.0f);
  const float half = size * 0.5f;

  // The hotspot is the middle, so the corner the quad starts from is half a size back along both
  // axes. Species's own winding and texture coordinates (MouseCursor::Render3D).
  const float originX = _hit.x - (rightX * half) + (frontX * half);
  const float originY = _hit.y - (rightY * half) + (frontY * half);
  const float originZ = _hit.z - (rightZ * half) + (frontZ * half);
  const auto at = [&](float _alongRight, float _alongFront, float _u, float _v)
  {
    return Vertex{originX + (rightX * _alongRight) - (frontX * _alongFront), originY + (rightY * _alongRight) - (frontY * _alongFront),
                  originZ + (rightZ * _alongRight) - (frontZ * _alongFront), _u, _v};
  };
  const Vertex a = at(0.0f, 0.0f, 0.0f, 1.0f);
  const Vertex b = at(size, 0.0f, 1.0f, 1.0f);
  const Vertex c = at(size, size, 1.0f, 0.0f);
  const Vertex d = at(0.0f, size, 0.0f, 0.0f);
  Emit(_list, _constants, {a, b, c, a, c, d}, _tint);
}

void CursorPass::DrawFootprint(ID3D12GraphicsCommandList* _list, D3D12_GPU_VIRTUAL_ADDRESS _constants, float _minX, float _minZ,
                               float _maxX, float _maxZ, float _groundHeight, const std::array<float, 4>& _tint)
{
  // AXIS-ALIGNED AND FLAT, because a footprint is cells and cells are axis-aligned, and because the
  // ground under one is about to be levelled: a ghost that followed the slope would show the shape
  // the ground has now rather than the shape it will have when the structure stands on it.
  const auto at = [&](float _x, float _z, float _u, float _v) { return Vertex{_x, _groundHeight, _z, _u, _v}; };
  const Vertex a = at(_minX, _minZ, 0.0f, 0.0f);
  const Vertex b = at(_maxX, _minZ, 1.0f, 0.0f);
  const Vertex c = at(_maxX, _maxZ, 1.0f, 1.0f);
  const Vertex d = at(_minX, _maxZ, 0.0f, 1.0f);
  Emit(_list, _constants, {a, b, c, a, c, d}, _tint);
}

} // namespace Neuron
