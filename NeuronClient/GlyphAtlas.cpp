#include "pch.h"

#include "GlyphAtlas.h"

#include <d3d12.h>
#include <dwrite.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>
#include <cmath>
#include <cstring>
#include <vector>

// `DWriteCreateFactory` is a free function and therefore an import, as `D3D12SerializeRootSignature` is
// in `MeshPass.cpp`. The link dependency travels with the code that needs it.
#pragma comment(lib, "dwrite.lib")

namespace Neuron
{

struct GlyphAtlasBinding
{
  winrt::com_ptr<ID3D12Resource> texture;
  winrt::com_ptr<ID3D12Resource> upload;
  winrt::com_ptr<ID3D12DescriptorHeap> shaderResourceHeap;

  std::array<GlyphTable, TEXT_SIZE_COUNT> glyphs{};
  std::array<float, TEXT_SIZE_COUNT> emSizePixels{};
  std::array<float, TEXT_SIZE_COUNT> lineHeightPixels{};
  std::array<FaceMetrics, TEXT_SIZE_COUNT> faces{};

  AtlasSlot solid{};

  std::uint32_t usedHeightPixels = 0;

  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
/// One byte a texel. Coverage is a single channel and always has been -- the whole reason ClearType's
/// three are averaged is to arrive here.
inline constexpr DXGI_FORMAT ATLAS_FORMAT = DXGI_FORMAT_R8_UNORM;

/// A missing entry, returned by reference for a character outside the range.
const GlyphEntry ABSENT_GLYPH{};

/// The block of full coverage that solid quads sample. Four texels square so that its middle is a
/// whole texel away from every edge, and packed first so it is always at the origin.
inline constexpr std::uint32_t SOLID_BLOCK_PIXELS = 4;

[[nodiscard]] float SrgbToLinear(float _encoded) noexcept
{
  return (_encoded <= 0.04045f) ? (_encoded / 12.92f) : std::pow((_encoded + 0.055f) / 1.055f, 2.4f);
}

[[nodiscard]] float LinearToSrgb(float _linear) noexcept
{
  return (_linear <= 0.0031308f) ? (_linear * 12.92f) : ((1.055f * std::pow(_linear, 1.0f / 2.4f)) - 0.055f);
}

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

/// The font face for `FONT_FAMILY` at Semibold, or nothing. **Nothing is a startup failure and never a
/// substitution** (ADR-009): `FindFamilyName` reporting no match is returned as a failure rather than
/// falling through to family zero, which is what a convenience implementation does and what would move
/// every string in the interface on a machine without Segoe UI.
[[nodiscard]] HRESULT OpenFontFace(winrt::com_ptr<IDWriteFactory>& _outFactory, winrt::com_ptr<IDWriteFontFace>& _outFace) noexcept
{
  winrt::com_ptr<IUnknown> unknownFactory;
  HRESULT opened = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, winrt::guid_of<IDWriteFactory>(), unknownFactory.put());
  if (FAILED(opened))
  {
    return opened;
  }
  if (!unknownFactory.try_as(_outFactory))
  {
    return E_NOINTERFACE;
  }

  winrt::com_ptr<IDWriteFontCollection> collection;
  opened = _outFactory->GetSystemFontCollection(collection.put(), FALSE);
  if (FAILED(opened))
  {
    return opened;
  }

  UINT32 index = 0;
  BOOL exists = FALSE;
  opened = collection->FindFamilyName(FONT_FAMILY.data(), &index, &exists);
  if (FAILED(opened))
  {
    return opened;
  }
  if (!exists)
  {
    // The one failure ADR-009 names by name.
    return DWRITE_E_NOFONT;
  }

  winrt::com_ptr<IDWriteFontFamily> family;
  opened = collection->GetFontFamily(index, family.put());
  if (FAILED(opened))
  {
    return opened;
  }

  winrt::com_ptr<IDWriteFont> font;
  opened = family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.put());
  if (FAILED(opened))
  {
    return opened;
  }

  return font->CreateFontFace(_outFace.put());
}
} // namespace

std::uint8_t AverageClearTypeCoverage(std::uint8_t _red, std::uint8_t _green, std::uint8_t _blue) noexcept
{
  constexpr float INVERSE = 1.0f / 255.0f;

  const float linear = (SrgbToLinear(static_cast<float>(_red) * INVERSE) + SrgbToLinear(static_cast<float>(_green) * INVERSE) +
                        SrgbToLinear(static_cast<float>(_blue) * INVERSE)) /
                       3.0f;

  const float encoded = LinearToSrgb(linear);
  const float scaled = (encoded * 255.0f) + 0.5f;

  if (scaled <= 0.0f)
  {
    return 0;
  }
  if (scaled >= 255.0f)
  {
    return 255;
  }
  return static_cast<std::uint8_t>(scaled);
}

float AdvancePixels(std::uint32_t _designAdvance, std::uint16_t _designUnitsPerEm, float _emSizePixels) noexcept
{
  if (_designUnitsPerEm == 0)
  {
    return 0.0f;
  }
  return (static_cast<float>(_designAdvance) * _emSizePixels) / static_cast<float>(_designUnitsPerEm);
}

GlyphAtlas::GlyphAtlas() noexcept
  : m_binding(std::make_shared<GlyphAtlasBinding>())
{
}

GlyphAtlas::~GlyphAtlas() noexcept = default;

bool GlyphAtlas::Create(const GraphicsDevice& _device, const FitTransform& _interfaceFit) noexcept
{
  GlyphAtlasBinding& binding = *m_binding;
  binding.ready = false;
  binding.lastHresult = S_OK;
  binding.usedHeightPixels = 0;
  binding.glyphs = {};
  binding.faces = {};
  binding.solid = {};

  winrt::com_ptr<ID3D12Device> device;
  if (!OpenDevice(_device, device) || (_interfaceFit.scale <= 0.0f))
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  winrt::com_ptr<IDWriteFactory> factory;
  winrt::com_ptr<IDWriteFontFace> face;
  binding.lastHresult = OpenFontFace(factory, face);
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  DWRITE_FONT_METRICS faceMetrics{};
  face->GetMetrics(&faceMetrics);
  if (faceMetrics.designUnitsPerEm == 0)
  {
    binding.lastHresult = E_UNEXPECTED;
    return false;
  }

  // === THE COVERAGE, ONE GLYPH AT A TIME ============================================== //
  //
  // The whole atlas is assembled in system memory first and uploaded once. A texture upload per glyph
  // would be 190 copies and 190 barriers for 512 KiB.
  std::vector<std::uint8_t> atlas(static_cast<std::size_t>(ATLAS_WIDTH_PIXELS) * ATLAS_HEIGHT_PIXELS, 0);

  AtlasPacker packer;
  packer.Reset(ATLAS_WIDTH_PIXELS, ATLAS_HEIGHT_PIXELS);

  // **THE SOLID BLOCK GOES IN FIRST**, so a plate and a glyph are one kind of quad and the interface is
  // one draw in emission order. Padded like a glyph so no glyph's edge can ever bleed into it.
  AtlasSlot solid{};
  if (!packer.Place(SOLID_BLOCK_PIXELS + (PADDING_PIXELS * 2), SOLID_BLOCK_PIXELS + (PADDING_PIXELS * 2), solid))
  {
    binding.lastHresult = E_OUTOFMEMORY;
    return false;
  }
  binding.solid = AtlasSlot{.left = solid.left + PADDING_PIXELS,
                            .top = solid.top + PADDING_PIXELS,
                            .widthPixels = SOLID_BLOCK_PIXELS,
                            .heightPixels = SOLID_BLOCK_PIXELS};
  for (std::uint32_t row = 0; row < SOLID_BLOCK_PIXELS; ++row)
  {
    for (std::uint32_t column = 0; column < SOLID_BLOCK_PIXELS; ++column)
    {
      atlas[(static_cast<std::size_t>(binding.solid.top + row) * ATLAS_WIDTH_PIXELS) + binding.solid.left + column] = 255;
    }
  }

  const std::array<std::int32_t, TEXT_SIZE_COUNT> authored{BODY_AUTHORED_PIXELS, DISPLAY_AUTHORED_PIXELS};

  for (std::size_t size = 0; size < TEXT_SIZE_COUNT; ++size)
  {
    // **THE PHYSICAL SIZE IS THE AUTHORED ONE THROUGH THE INTERFACE FIT** (ADR-011). At the target
    // device's exact 2x this is 40 and 64, which is what "neither doubled nor resampled" means.
    const float emSize = static_cast<float>(authored[size]) * _interfaceFit.scale;
    binding.emSizePixels[size] = emSize;

    const float perDesignUnit = emSize / static_cast<float>(faceMetrics.designUnitsPerEm);
    binding.lineHeightPixels[size] = static_cast<float>(faceMetrics.ascent + faceMetrics.descent + faceMetrics.lineGap) * perDesignUnit;
    binding.faces[size] = FaceMetrics{.emSizePixels = emSize,
                                      .ascentPixels = static_cast<float>(faceMetrics.ascent) * perDesignUnit,
                                      .descentPixels = static_cast<float>(faceMetrics.descent) * perDesignUnit};

    for (std::size_t index = 0; index < CHARACTER_COUNT; ++index)
    {
      const wchar_t character = static_cast<wchar_t>(FIRST_CHARACTER + static_cast<wchar_t>(index));
      const UINT32 codepoint = static_cast<UINT32>(character);

      UINT16 glyphIndex = 0;
      if (FAILED(face->GetGlyphIndices(&codepoint, 1, &glyphIndex)) || (glyphIndex == 0))
      {
        // No glyph for this character in this face. Left absent, which draws nothing.
        continue;
      }

      DWRITE_GLYPH_METRICS designMetrics{};
      if (FAILED(face->GetDesignGlyphMetrics(&glyphIndex, 1, &designMetrics, FALSE)))
      {
        continue;
      }

      GlyphEntry& entry = binding.glyphs[size][index];
      entry.advancePixels = AdvancePixels(designMetrics.advanceWidth, faceMetrics.designUnitsPerEm, emSize);
      entry.present = true;

      // A run of exactly one glyph, at the origin. The baseline origin is zero so the bounds that come
      // back are relative to the pen, which is what the bearings below are.
      FLOAT advance = entry.advancePixels;
      DWRITE_GLYPH_OFFSET offset{};
      DWRITE_GLYPH_RUN run{};
      run.fontFace = face.get();
      run.fontEmSize = emSize;
      run.glyphCount = 1;
      run.glyphIndices = &glyphIndex;
      run.glyphAdvances = &advance;
      run.glyphOffsets = &offset;
      run.isSideways = FALSE;
      run.bidiLevel = 0;

      winrt::com_ptr<IDWriteGlyphRunAnalysis> analysis;
      // **`pixelsPerDip` IS ONE AND THE SCALE IS ALREADY IN `fontEmSize`.** Putting the fit's scale here
      // instead would scale the rasterization and not the metrics, so the advances and the coverage
      // would disagree -- which draws correctly shaped glyphs at steadily wrong positions.
      const HRESULT analysed = factory->CreateGlyphRunAnalysis(&run, 1.0f, nullptr, DWRITE_RENDERING_MODE_NATURAL,
                                                               DWRITE_MEASURING_MODE_NATURAL, 0.0f, 0.0f, analysis.put());
      if (FAILED(analysed))
      {
        continue;
      }

      RECT bounds{};
      if (FAILED(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds)))
      {
        continue;
      }

      const std::int32_t inkWidth = bounds.right - bounds.left;
      const std::int32_t inkHeight = bounds.bottom - bounds.top;
      if ((inkWidth <= 0) || (inkHeight <= 0))
      {
        // A space. It has an advance and no coverage, which is not a failure.
        continue;
      }

      // Three bytes a texel: ClearType is one per subpixel across.
      std::vector<std::uint8_t> coverage(static_cast<std::size_t>(inkWidth) * inkHeight * 3, 0);
      if (FAILED(
            analysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds, coverage.data(), static_cast<UINT32>(coverage.size()))))
      {
        continue;
      }

      AtlasSlot slot{};
      if (!packer.Place(static_cast<std::uint32_t>(inkWidth) + (PADDING_PIXELS * 2),
                        static_cast<std::uint32_t>(inkHeight) + (PADDING_PIXELS * 2), slot))
      {
        // The atlas is full. Every remaining glyph would fail the same way, so this is a failure of the
        // atlas rather than of the character -- reported rather than skipped.
        binding.lastHresult = E_OUTOFMEMORY;
        return false;
      }

      entry.slot.left = slot.left + PADDING_PIXELS;
      entry.slot.top = slot.top + PADDING_PIXELS;
      entry.slot.widthPixels = static_cast<std::uint32_t>(inkWidth);
      entry.slot.heightPixels = static_cast<std::uint32_t>(inkHeight);
      entry.bearingXPixels = static_cast<float>(bounds.left);
      entry.bearingYPixels = static_cast<float>(bounds.top);

      for (std::int32_t row = 0; row < inkHeight; ++row)
      {
        for (std::int32_t column = 0; column < inkWidth; ++column)
        {
          const std::size_t source = ((static_cast<std::size_t>(row) * inkWidth) + column) * 3;
          const std::size_t destination = (static_cast<std::size_t>(entry.slot.top + row) * ATLAS_WIDTH_PIXELS) + entry.slot.left + column;
          atlas[destination] = AverageClearTypeCoverage(coverage[source], coverage[source + 1], coverage[source + 2]);
        }
      }
    }
  }

  binding.usedHeightPixels = packer.UsedHeightPixels();

  // === THE TEXTURE ==================================================================== //

  const D3D12_HEAP_PROPERTIES defaultHeap{.Type = D3D12_HEAP_TYPE_DEFAULT,
                                          .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                          .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                          .CreationNodeMask = 1,
                                          .VisibleNodeMask = 1};

  const D3D12_RESOURCE_DESC description{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                        .Alignment = 0,
                                        .Width = ATLAS_WIDTH_PIXELS,
                                        .Height = ATLAS_HEIGHT_PIXELS,
                                        .DepthOrArraySize = 1,
                                        .MipLevels = 1,
                                        .Format = ATLAS_FORMAT,
                                        .SampleDesc = {.Count = 1, .Quality = 0},
                                        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                        .Flags = D3D12_RESOURCE_FLAG_NONE};

  binding.lastHresult = device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, winrt::guid_of<ID3D12Resource>(), binding.texture.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  // **THE UPLOAD BUFFER'S ROWS ARE 256-BYTE ALIGNED**, which a 1024-wide single-channel texture happens
  // to satisfy exactly -- but the alignment is asked for rather than assumed, because a later atlas
  // width that is not a multiple of 256 would otherwise upload a sheared glyph sheet and nothing would
  // say so.
  UINT64 uploadBytes = 0;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT rowCount = 0;
  UINT64 rowBytes = 0;
  device->GetCopyableFootprints(&description, 0, 1, 0, &footprint, &rowCount, &rowBytes, &uploadBytes);

  const D3D12_HEAP_PROPERTIES uploadHeap{.Type = D3D12_HEAP_TYPE_UPLOAD,
                                         .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                         .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                         .CreationNodeMask = 1,
                                         .VisibleNodeMask = 1};

  const D3D12_RESOURCE_DESC uploadDescription{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                              .Alignment = 0,
                                              .Width = uploadBytes,
                                              .Height = 1,
                                              .DepthOrArraySize = 1,
                                              .MipLevels = 1,
                                              .Format = DXGI_FORMAT_UNKNOWN,
                                              .SampleDesc = {.Count = 1, .Quality = 0},
                                              .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                              .Flags = D3D12_RESOURCE_FLAG_NONE};

  binding.lastHresult =
    device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                    winrt::guid_of<ID3D12Resource>(), binding.upload.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  void* mapped = nullptr;
  const D3D12_RANGE noRead{.Begin = 0, .End = 0};
  binding.lastHresult = binding.upload->Map(0, &noRead, &mapped);
  if (FAILED(binding.lastHresult) || (mapped == nullptr))
  {
    return false;
  }

  std::uint8_t* const destination = static_cast<std::uint8_t*>(mapped) + footprint.Offset;
  for (std::uint32_t row = 0; row < ATLAS_HEIGHT_PIXELS; ++row)
  {
    std::memcpy(destination + (static_cast<std::size_t>(row) * footprint.Footprint.RowPitch),
                atlas.data() + (static_cast<std::size_t>(row) * ATLAS_WIDTH_PIXELS), ATLAS_WIDTH_PIXELS);
  }
  binding.upload->Unmap(0, nullptr);

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  D3D12_TEXTURE_COPY_LOCATION source{};
  source.pResource = binding.upload.get();
  source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  source.PlacedFootprint = footprint;

  D3D12_TEXTURE_COPY_LOCATION target{};
  target.pResource = binding.texture.get();
  target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  target.SubresourceIndex = 0;

  commandList->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);

  D3D12_RESOURCE_BARRIER toShaderResource{};
  toShaderResource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  toShaderResource.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  toShaderResource.Transition.pResource = binding.texture.get();
  toShaderResource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  toShaderResource.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  toShaderResource.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  commandList->ResourceBarrier(1, &toShaderResource);

  // === THE VIEW ======================================================================= //

  const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .NumDescriptors = 1, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, .NodeMask = 0};
  binding.lastHresult =
    device->CreateDescriptorHeap(&heapDescription, winrt::guid_of<ID3D12DescriptorHeap>(), binding.shaderResourceHeap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC shaderView{};
  shaderView.Format = ATLAS_FORMAT;
  shaderView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  shaderView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  shaderView.Texture2D.MostDetailedMip = 0;
  shaderView.Texture2D.MipLevels = 1;
  shaderView.Texture2D.ResourceMinLODClamp = 0.0f;
  device->CreateShaderResourceView(binding.texture.get(), &shaderView, binding.shaderResourceHeap->GetCPUDescriptorHandleForHeapStart());

  binding.ready = true;
  return true;
}

void GlyphAtlas::Destroy() noexcept
{
  GlyphAtlasBinding& binding = *m_binding;
  binding.shaderResourceHeap = nullptr;
  binding.upload = nullptr;
  binding.texture = nullptr;
  binding.glyphs = {};
  binding.emSizePixels = {};
  binding.lineHeightPixels = {};
  binding.faces = {};
  binding.solid = {};
  binding.usedHeightPixels = 0;
  binding.ready = false;
}

bool GlyphAtlas::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t GlyphAtlas::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

float GlyphAtlas::EmSizePixels(TextSize _size) const noexcept
{
  const std::size_t index = static_cast<std::size_t>(_size);
  return (index < TEXT_SIZE_COUNT) ? m_binding->emSizePixels[index] : 0.0f;
}

float GlyphAtlas::LineHeightPixels(TextSize _size) const noexcept
{
  const std::size_t index = static_cast<std::size_t>(_size);
  return (index < TEXT_SIZE_COUNT) ? m_binding->lineHeightPixels[index] : 0.0f;
}

FaceMetrics GlyphAtlas::Face(TextSize _size) const noexcept
{
  const std::size_t index = static_cast<std::size_t>(_size);
  return (index < TEXT_SIZE_COUNT) ? m_binding->faces[index] : FaceMetrics{};
}

AtlasSlot GlyphAtlas::SolidSlot() const noexcept
{
  return m_binding->solid;
}

const GlyphTable& GlyphAtlas::Table(TextSize _size) const noexcept
{
  const std::size_t index = static_cast<std::size_t>(_size);
  return m_binding->glyphs[(index < TEXT_SIZE_COUNT) ? index : 0];
}

const GlyphEntry& GlyphAtlas::Glyph(TextSize _size, wchar_t _character) const noexcept
{
  const std::size_t size = static_cast<std::size_t>(_size);
  if ((size >= TEXT_SIZE_COUNT) || (_character < FIRST_CHARACTER) || (_character > LAST_CHARACTER))
  {
    return ABSENT_GLYPH;
  }
  return m_binding->glyphs[size][static_cast<std::size_t>(_character - FIRST_CHARACTER)];
}

float GlyphAtlas::MeasureTextPixels(TextSize _size, std::wstring_view _text) const noexcept
{
  float width = 0.0f;
  for (const wchar_t character : _text)
  {
    width += Glyph(_size, character).advancePixels;
  }
  return width;
}

std::uint32_t GlyphAtlas::WidthPixels() const noexcept
{
  return ATLAS_WIDTH_PIXELS;
}

std::uint32_t GlyphAtlas::HeightPixels() const noexcept
{
  return ATLAS_HEIGHT_PIXELS;
}

std::uint32_t GlyphAtlas::UsedHeightPixels() const noexcept
{
  return m_binding->usedHeightPixels;
}

void* GlyphAtlas::ShaderResourceHeapUnknown() const noexcept
{
  return m_binding->shaderResourceHeap.get();
}

std::uint64_t GlyphAtlas::ShaderResourceHandle() const noexcept
{
  return (m_binding->shaderResourceHeap != nullptr) ? m_binding->shaderResourceHeap->GetGPUDescriptorHandleForHeapStart().ptr : 0;
}

} // namespace Neuron
