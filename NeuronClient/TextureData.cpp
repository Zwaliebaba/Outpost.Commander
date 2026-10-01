#include "pch.h"
#include "TextureData.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace
{
// The .dds layout. Sizes and offsets are in bytes, from the start of the file.
constexpr std::array<std::uint8_t, 4> DDS_MAGIC{'D', 'D', 'S', ' '};
constexpr std::uint32_t HEADER_BYTES = 124;
constexpr size_t LEGACY_DATA_OFFSET = 4 + HEADER_BYTES;
constexpr size_t DX10_HEADER_BYTES = 20;
constexpr size_t HEIGHT_OFFSET = 12;
constexpr size_t WIDTH_OFFSET = 16;
constexpr size_t MIP_COUNT_OFFSET = 28;
constexpr size_t PIXEL_FORMAT_FLAGS_OFFSET = 80;
constexpr size_t FOUR_CC_OFFSET = 84;
constexpr size_t BIT_COUNT_OFFSET = 88;
constexpr size_t RED_MASK_OFFSET = 92;
constexpr size_t GREEN_MASK_OFFSET = 96;
constexpr size_t BLUE_MASK_OFFSET = 100;
constexpr size_t ALPHA_MASK_OFFSET = 104;
constexpr size_t CAPS2_OFFSET = 112;
constexpr std::uint32_t FLAGS_MIP_COUNT = 0x20000;
constexpr size_t FLAGS_OFFSET = 8;
constexpr std::uint32_t PIXEL_FOUR_CC = 0x4;
constexpr std::uint32_t PIXEL_RGB = 0x40;
constexpr std::uint32_t PIXEL_ALPHA = 0x1;
constexpr std::uint32_t CAPS2_CUBE_MAP = 0x200;
constexpr std::uint32_t CAPS2_VOLUME = 0x200000;
constexpr std::array<std::uint8_t, 4> FOUR_CC_DX10{'D', 'X', '1', '0'};
// The DX10 header: the format, the dimension, the flags, the array size.
constexpr std::uint32_t DIMENSION_TEXTURE_2D = 3;
constexpr std::uint32_t MISC_CUBE_MAP = 0x4;
constexpr UINT TEXEL_BYTES = 4;

std::uint32_t ReadUInt32(std::span<const std::uint8_t> _bytes, size_t _offset) noexcept
{
  std::uint32_t value = 0;
  std::memcpy(&value, _bytes.data() + _offset, sizeof(value));
  return value;
}

[[noreturn]] void Fail(std::string_view _fileName, std::string_view _message)
{
  throw Neuron::Exception(std::format("The texture {} cannot be read: {}.", _fileName, _message));
}

// The format of a legacy header's 32-bit texels with alpha, from where its channels sit.
DXGI_FORMAT LegacyFormat(std::span<const std::uint8_t> _bytes) noexcept
{
  const std::uint32_t flags = ReadUInt32(_bytes, PIXEL_FORMAT_FLAGS_OFFSET);
  if ((flags & PIXEL_RGB) == 0 || (flags & PIXEL_ALPHA) == 0 || ReadUInt32(_bytes, BIT_COUNT_OFFSET) != 32 ||
      ReadUInt32(_bytes, ALPHA_MASK_OFFSET) != 0xFF00'0000 || ReadUInt32(_bytes, GREEN_MASK_OFFSET) != 0x0000'FF00)
    return DXGI_FORMAT_UNKNOWN;
  const std::uint32_t red = ReadUInt32(_bytes, RED_MASK_OFFSET);
  const std::uint32_t blue = ReadUInt32(_bytes, BLUE_MASK_OFFSET);
  if (red == 0x00FF'0000 && blue == 0x0000'00FF)
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  if (red == 0x0000'00FF && blue == 0x00FF'0000)
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  return DXGI_FORMAT_UNKNOWN;
}

bool IsSupported(DXGI_FORMAT _format) noexcept
{
  return _format == DXGI_FORMAT_R8G8B8A8_UNORM || _format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || _format == DXGI_FORMAT_B8G8R8A8_UNORM ||
         _format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}
} // namespace

Neuron::TextureData Neuron::ParseDds(std::span<const std::uint8_t> _bytes, std::string_view _fileName)
{
  if (_bytes.size() < LEGACY_DATA_OFFSET || !std::equal(DDS_MAGIC.begin(), DDS_MAGIC.end(), _bytes.begin()) ||
      ReadUInt32(_bytes, 4) != HEADER_BYTES)
    Fail(_fileName, "it is not a .dds file");
  if ((ReadUInt32(_bytes, CAPS2_OFFSET) & (CAPS2_CUBE_MAP | CAPS2_VOLUME)) != 0)
    Fail(_fileName, "it is a cube map or a volume, not a 2D texture");

  TextureData texture;
  texture.width = ReadUInt32(_bytes, WIDTH_OFFSET);
  texture.height = ReadUInt32(_bytes, HEIGHT_OFFSET);
  if (texture.width == 0 || texture.height == 0)
    Fail(_fileName, "it is empty");

  size_t offset = LEGACY_DATA_OFFSET;
  const bool fourCc = (ReadUInt32(_bytes, PIXEL_FORMAT_FLAGS_OFFSET) & PIXEL_FOUR_CC) != 0;
  if (fourCc && std::equal(FOUR_CC_DX10.begin(), FOUR_CC_DX10.end(), _bytes.begin() + FOUR_CC_OFFSET))
  {
    if (_bytes.size() < LEGACY_DATA_OFFSET + DX10_HEADER_BYTES)
      Fail(_fileName, "the file ends early");
    texture.format = static_cast<DXGI_FORMAT>(ReadUInt32(_bytes, LEGACY_DATA_OFFSET));
    if (ReadUInt32(_bytes, LEGACY_DATA_OFFSET + 4) != DIMENSION_TEXTURE_2D ||
        (ReadUInt32(_bytes, LEGACY_DATA_OFFSET + 8) & MISC_CUBE_MAP) != 0 || ReadUInt32(_bytes, LEGACY_DATA_OFFSET + 12) > 1)
      Fail(_fileName, "it is not a single 2D texture");
    offset += DX10_HEADER_BYTES;
  }
  else if (!fourCc)
  {
    texture.format = LegacyFormat(_bytes);
  }
  if (!IsSupported(texture.format))
    Fail(_fileName, "its texels are not 8-bit BGRA or RGBA");

  const std::uint32_t mipCount =
    (ReadUInt32(_bytes, FLAGS_OFFSET) & FLAGS_MIP_COUNT) != 0 ? std::max(1u, ReadUInt32(_bytes, MIP_COUNT_OFFSET)) : 1u;
  UINT width = texture.width;
  UINT height = texture.height;
  for (std::uint32_t level = 0; level < mipCount; ++level)
  {
    const size_t levelBytes = size_t{width} * height * TEXEL_BYTES;
    if (levelBytes > _bytes.size() - offset)
      Fail(_fileName, "the file ends early");
    texture.levels.emplace_back(_bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                _bytes.begin() + static_cast<std::ptrdiff_t>(offset + levelBytes));
    offset += levelBytes;
    width = std::max(1u, width / 2);
    height = std::max(1u, height / 2);
  }
  if (offset != _bytes.size())
    Fail(_fileName, "it has bytes left over");
  return texture;
}

void Neuron::BuildMipLevels(TextureData& _texture)
{
  _texture.levels.resize(1);
  UINT width = _texture.width;
  UINT height = _texture.height;
  while (width > 1 || height > 1)
  {
    const ByteBuffer& above = _texture.levels.back();
    const UINT nextWidth = std::max(1u, width / 2);
    const UINT nextHeight = std::max(1u, height / 2);
    ByteBuffer next(size_t{nextWidth} * nextHeight * TEXEL_BYTES);
    // A texel above, held at the edge so that a side of 1 averages with itself.
    const auto texel = [&](UINT _x, UINT _y, UINT _channel)
    { return UINT{above[((size_t{std::min(_y, height - 1)} * width) + std::min(_x, width - 1)) * TEXEL_BYTES + _channel]}; };
    for (UINT y = 0; y < nextHeight; ++y)
    {
      for (UINT x = 0; x < nextWidth; ++x)
      {
        for (UINT channel = 0; channel < TEXEL_BYTES; ++channel)
        {
          const UINT sum = texel(2 * x, 2 * y, channel) + texel((2 * x) + 1, 2 * y, channel) + texel(2 * x, (2 * y) + 1, channel) +
                           texel((2 * x) + 1, (2 * y) + 1, channel);
          next[(((size_t{y} * nextWidth) + x) * TEXEL_BYTES) + channel] = static_cast<std::uint8_t>((sum + 2) / 4);
        }
      }
    }
    // The reference above dangles once the levels grow, so it is not used after this.
    _texture.levels.push_back(std::move(next));
    width = nextWidth;
    height = nextHeight;
  }
}
