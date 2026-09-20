#pragma once

#include "RenderView.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

// The minimap's picture, as arithmetic (Design/Interface.md §9.2; m1-vertical-slice/K4). Its client
// area is exactly 256x256, so a Small landscape of 128 cells is TWO PIXELS A CELL and is drawn
// without resampling - which is why the panel is that size and why M1 is a Small landscape.
//
// NO DIRECT3D HERE, for the reason NeuronClient/GroundRay.h and NeuronClient/CapturePath.h give: everything
// below is a loop over a fog grid and a list of instances, so a minimap that draws a commander's
// army in the wrong place is a failing assertion on any machine rather than something only a
// screenshot can show. What the pass above it does is upload these bytes and draw one quad.
//
// IT IS FED THE RENDER VIEW AND NOTHING ELSE, which is §9.2's first line in as many words: the fog
// it draws is "the same grid the fog pass reads, so the minimap and the world can never disagree",
// and the objects it draws are the render view's instances, which are what the interest set holds -
// so the minimap cannot show a commander something he could not see in the world.

namespace Neuron
{

/// The client area §9.2 fixes. A landscape of this many cells or fewer is drawn at a whole number
/// of pixels a cell; §11 hands the scale rule for the bigger ones to the milestone that ships them.
inline constexpr std::uint32_t MINIMAP_PIXELS = 256;
inline constexpr std::uint32_t MINIMAP_BYTES = MINIMAP_PIXELS * MINIMAP_PIXELS * 4;

/// One thing on the map. THE CALLER NAMES WHAT IT DRAWS rather than handing over the render view,
/// which is PlacementPreview.h's rule and has a reason here beyond testability: §9.2 wants
/// "structures, two pixels a cell of their footprint" and "devices, one pixel each", and a
/// RenderInstance says neither - it carries a model, a pose and a scale, and telling a factory from
/// a truck by its scale would be a guess that breaks the first time a model is resized. The replica
/// knows which is which, and the executable that holds both is where they meet.
struct MinimapBlip
{
  float x = 0.0f; ///< World units
  float z = 0.0f;
  /// The footprint in CELLS. Zero is a device, which is one pixel whatever the map's scale.
  std::uint32_t cellsX = 0;
  std::uint32_t cellsY = 0;
  std::uint8_t colorIndex = 0;
  bool selected = false;
  /// A structure the commander remembers rather than sees, drawn at half brightness (§9.2).
  bool ghost = false;
};

/// What the minimap is drawn from. The colours arrive packed as PackedRgba8 puts them, because the
/// commander palette is Content's and this is Neuron (AGENTS.md R9).
struct MinimapView
{
  const FogView* fog = nullptr;
  std::span<const MinimapBlip> blips;
  std::span<const std::uint32_t> commanderColors;
  std::uint32_t terrainColor = 0; ///< What explored-but-unseen ground is tinted from
  std::uint32_t accentColor = 0;  ///< §9.2's selection colour
  std::uint32_t borderColor = 0;  ///< The camera's quadrilateral
  float worldUnitsPerCell = 64.0f;
  /// The four far corners of the frustum where they meet the ground plane, in world units, in
  /// order round the quadrilateral. All zero draws none, which is what a frame before the join has.
  std::array<float, 8> frustumGround{};
};

/// Fills _outPixels with MINIMAP_BYTES of RGBA8, top row first, drawn back to front as §9.2 lists:
/// the fog, the structures, the devices, the selection, then the camera's quadrilateral.
void BuildMinimap(const MinimapView& _view, std::vector<std::uint8_t>& _outPixels);

/// Where a minimap pixel lands on the landscape, in world units. The inverse of what BuildMinimap
/// draws with, so that a click goes to the point under it (§9.2's left click).
struct MinimapPoint
{
  float x = 0.0f;
  float z = 0.0f;
  bool inside = false;
};

[[nodiscard]] MinimapPoint WorldOfMinimap(std::int32_t _pixelX, std::int32_t _pixelY, std::uint32_t _cellsPerSide,
                                          float _worldUnitsPerCell) noexcept;

} // namespace Neuron
