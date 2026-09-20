#pragma once

#include "HeightView.h"
#include "TextureFile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Neuron
{

/// A chunk is 32 by 32 cells, 129 by 129 samples at four samples a cell (TechnicalDesign.md §6.2),
/// drawn at a stride of 1, 2, 4 or 8 samples with a skirt hanging from its border to hide the
/// cracks between strides.
inline constexpr std::uint32_t CHUNK_CELLS = 32;
inline constexpr std::uint32_t CHUNK_STEPS = CHUNK_CELLS * 4;
inline constexpr std::uint32_t CHUNK_SAMPLES = CHUNK_STEPS + 1;
inline constexpr std::array<std::uint32_t, 4> CHUNK_STRIDES = {1, 2, 4, 8};
/// How far below the water the skirt hangs, in world units.
inline constexpr float SKIRT_DEPTH = 64.0f;
/// The Species rule for the shore (SpeciesTerrain.md §6): a vertex under this much above the water
/// is pushed to this much below it, so that the shore dips under the plane with no seam.
inline constexpr float SHORE_LIP = 0.3f;
inline constexpr float SHORE_DIP = 10.0f;

/// Position and colour: the face normal comes from the pixel shader's derivatives, and the colour
/// is flat per triangle from its first vertex (Lighting.h says what the alpha means).
struct TerrainVertex
{
  float x;
  float y;
  float z;
  std::uint32_t color; ///< RGBA8, R in the low byte
};

/// The 64 by 64 palette the Species formula indexes (SpeciesTerrain.md §6): slope along x from flat
/// to cliff, height along y from summit to sea level. Built in until Core's TextureFile reads one
/// from GameData\Terrain in M1.
class TerrainPalette
{
public:
  static constexpr std::uint32_t SIDE = 64;

  [[nodiscard]] static TerrainPalette BuiltIn();

  /// Fills a palette from a decoded texture, which must be SIDE by SIDE. False, with _out
  /// untouched, for anything else, so that a bad or missing file falls back to BuiltIn rather than
  /// colouring the landscape with whatever it found. Row 0 is the summit and row 63 sea level, as
  /// Lookup indexes it and as Tools/MakeTerrainPalette.py writes it.
  [[nodiscard]] static bool FromTexture(const TextureFile& _texture, TerrainPalette& _out);

  /// _u the slope term and _v the height term, both 0 to 1, clamped.
  [[nodiscard]] std::uint32_t Lookup(float _u, float _v) const noexcept;

  std::array<std::uint32_t, static_cast<std::size_t>(SIDE) * SIDE> colors{};
};

struct TerrainMesh
{
  std::vector<TerrainVertex> vertices;
  std::vector<std::uint16_t> indices;
  float minX;
  float minY;
  float minZ;
  float maxX;
  float maxY;
  float maxZ;
  bool hasWater; ///< Some sample of the chunk lies below the water level
};

/// The steps a chunk spans at a stride, and the samples along one of its sides.
[[nodiscard]] constexpr std::uint32_t ChunkSteps(std::uint32_t _stride) noexcept
{
  return CHUNK_STEPS / (_stride < 1 ? 1 : (_stride > 8 ? 8 : _stride));
}

/// How many vertices a chunk at this stride has: the grid, plus two skirt vertices for each of the
/// four borders' edges. It depends on the STRIDE ALONE and never on the ground, which is what lets
/// a rebuilt chunk go back into the range it already occupies (m1-vertical-slice/K6).
[[nodiscard]] constexpr std::uint32_t ChunkVertexCount(std::uint32_t _stride) noexcept
{
  const std::uint32_t steps = ChunkSteps(_stride);
  return (steps + 1) * (steps + 1) + 8 * steps;
}

/// How many indices a chunk at this stride can ever need: every quad of the grid drawn, plus the
/// skirt.
///
/// "CAN EVER NEED" AND NOT "HAS", WHICH IS THE ONE PLACE THE HEIGHTS DO CHANGE A COUNT. The grid's
/// vertices are fixed, but BuildTerrainChunk SKIPS a quad whose four corners all lie at or below
/// the water (SpeciesTerrain.md §6's underwater plain), so a flatten that lifts ground out of the
/// sea - or drops it in - changes how many indices a chunk emits. m1-vertical-slice/K6's acceptance
/// says "a chunk's vertex and index counts are fixed by its stride"; the vertex count is, and the
/// index count is not. So each chunk's index range is allocated at this maximum and the level's own
/// indexCount says how much of it is drawn, which is a field the draw already reads. The cost is
/// about 8% of the index buffer on the slice landscape, because most quads are above water anyway.
[[nodiscard]] constexpr std::uint32_t MaxChunkIndexCount(std::uint32_t _stride) noexcept
{
  const std::uint32_t steps = ChunkSteps(_stride);
  return 6 * steps * steps + 24 * steps;
}

/// The chunks a side of a view, which rounds the samples up to whole chunks.
[[nodiscard]] std::uint32_t ChunksPerSide(const HeightView& _view) noexcept;

/// The mesh of chunk (_chunkX, _chunkY) at _stride, every vertex coloured on the CPU by the Species
/// formula: u = (1 - slope)^0.4, v = 1 - height / highest plus a seeded noise, into the palette.
/// Quads whose four corners all lie at or below the water are skipped, as Species skips the
/// underwater plain. Sixteen-bit indices: at most 17,157 vertices at stride 1.
[[nodiscard]] TerrainMesh BuildTerrainChunk(const HeightView& _view, std::uint32_t _chunkX, std::uint32_t _chunkY, std::uint32_t _stride,
                                            const TerrainPalette& _palette);

} // namespace Neuron
