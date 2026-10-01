#pragma once

namespace Neuron
{
// A 2D texture on the CPU, with its mip levels, largest first: what a .dds file holds (ADR-022). Every level's rows are
// packed with no gap, four bytes a texel.
struct TextureData
{
  UINT width = 0;
  UINT height = 0;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  std::vector<ByteBuffer> levels;
};

// Reads the bytes of a .dds file holding an uncompressed 2D texture of 8-bit BGRA or RGBA texels, in the legacy
// header or the DX10 one, with its mip levels if it has any. Throws Neuron::Exception naming _fileName when the file is
// not a .dds file, is cut short, or holds a texture of any other kind: compressed, a cube map, a volume or an array.
[[nodiscard]] TextureData ParseDds(std::span<const std::uint8_t> _bytes, std::string_view _fileName);

// Replaces a texture's levels below its largest with a full chain down to 1 by 1, each the average of the 2 by 2
// texels above it, channel by channel. That is right for the alpha of a white sprite, which is all the sky reads
// (ADR-022); a color with straight alpha would want the color weighted by its alpha.
void BuildMipLevels(TextureData& _texture);
} // namespace Neuron
