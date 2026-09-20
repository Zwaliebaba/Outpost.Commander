#include "pch.h"

#include "Minimap.h"

#include <array>
#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The minimap's picture (Design/Interface.md §9.2; m1-vertical-slice/K4). It is the one place a
// commander sees the whole landscape at once, and every way it can lie - drawing ground he has not
// explored, putting his army in the wrong half, hiding a device under a building, answering a click
// with a point he did not click - is a loop over integers and is decided here.
namespace ClientTests
{

namespace
{

constexpr std::uint32_t CELLS = 128; ///< A Small landscape: two pixels a cell on a 256 map
constexpr float WORLD_UNITS_PER_CELL = 64.0f;
constexpr std::uint32_t TERRAIN = 0xFF40C080u; ///< Packed RGBA: r 0x80, g 0xC0, b 0x40
constexpr std::uint32_t ACCENT = 0xFF00FFFFu;
constexpr std::uint32_t BORDER = 0xFFFF0000u;
constexpr std::uint32_t RED = 0xFF0000FFu;
constexpr std::uint32_t BLUE = 0xFFFF0000u;

/// A fog grid of one shade throughout, which most of these cases want.
[[nodiscard]] Neuron::FogView Fog(Neuron::FogShade _shade)
{
  Neuron::FogView fog{};
  fog.cellsPerSide = CELLS;
  fog.cells.assign(static_cast<std::size_t>(CELLS) * CELLS, static_cast<std::uint8_t>(_shade));
  return fog;
}

[[nodiscard]] std::uint32_t PixelAt(const std::vector<std::uint8_t>& _pixels, std::int32_t _x, std::int32_t _y)
{
  const std::size_t at = ((static_cast<std::size_t>(_y) * Neuron::MINIMAP_PIXELS) + static_cast<std::size_t>(_x)) * 4;
  return static_cast<std::uint32_t>(_pixels[at]) | (static_cast<std::uint32_t>(_pixels[at + 1]) << 8) |
         (static_cast<std::uint32_t>(_pixels[at + 2]) << 16) | (static_cast<std::uint32_t>(_pixels[at + 3]) << 24);
}

[[nodiscard]] std::uint32_t CountOf(const std::vector<std::uint8_t>& _pixels, std::uint32_t _color)
{
  std::uint32_t count = 0;
  for (std::uint32_t y = 0; y < Neuron::MINIMAP_PIXELS; ++y)
  {
    for (std::uint32_t x = 0; x < Neuron::MINIMAP_PIXELS; ++x)
    {
      count += PixelAt(_pixels, static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)) == _color ? 1u : 0u;
    }
  }
  return count;
}

[[nodiscard]] Neuron::MinimapView ViewOf(const Neuron::FogView& _fog, std::span<const Neuron::MinimapBlip> _blips,
                                         std::span<const std::uint32_t> _palette)
{
  Neuron::MinimapView view{};
  view.fog = &_fog;
  view.blips = _blips;
  view.commanderColors = _palette;
  view.terrainColor = TERRAIN;
  view.accentColor = ACCENT;
  view.borderColor = BORDER;
  view.worldUnitsPerCell = WORLD_UNITS_PER_CELL;
  return view;
}

[[nodiscard]] Neuron::MinimapBlip Device(float _x, float _z, std::uint8_t _color, bool _selected = false)
{
  Neuron::MinimapBlip blip{};
  blip.x = _x;
  blip.z = _z;
  blip.colorIndex = _color;
  blip.selected = _selected;
  return blip;
}

[[nodiscard]] Neuron::MinimapBlip Structure(float _x, float _z, std::uint32_t _cellsX, std::uint32_t _cellsY, std::uint8_t _color)
{
  Neuron::MinimapBlip blip = Device(_x, _z, _color);
  blip.cellsX = _cellsX;
  blip.cellsY = _cellsY;
  return blip;
}

} // namespace

TEST_CLASS(MinimapTests)
{
public:
  TEST_METHOD(UnexploredGroundIsBlackAndNothingElseIs)
  {
    // The one thing the minimap exists not to do: show a commander ground he has never scouted.
    const Neuron::FogView fog = Fog(Neuron::FogShade::Unexplored);
    const std::array<std::uint32_t, 2> palette{RED, BLUE};
    std::vector<std::uint8_t> pixels;
    Neuron::BuildMinimap(ViewOf(fog, {}, palette), pixels);
    Assert::AreEqual(std::size_t{Neuron::MINIMAP_BYTES}, pixels.size(), L"the map is the size §9.2 fixes");
    Assert::AreEqual(Neuron::MINIMAP_PIXELS * Neuron::MINIMAP_PIXELS, CountOf(pixels, 0xFF000000u), L"every pixel is black");
  }

  TEST_METHOD(ExploredGroundIsHalfAsBrightAsVisibleGround)
  {
    // §9.2: "black where unexplored, the terrain colour at half brightness where explored, at full
    // where visible". Three states and three answers, which is what makes the fog readable at all.
    Neuron::FogView fog = Fog(Neuron::FogShade::Unexplored);
    fog.cells[0] = static_cast<std::uint8_t>(Neuron::FogShade::Visible);
    fog.cells[1] = static_cast<std::uint8_t>(Neuron::FogShade::Explored);
    std::vector<std::uint8_t> pixels;
    const std::array<std::uint32_t, 1> palette{RED};
    Neuron::BuildMinimap(ViewOf(fog, {}, palette), pixels);
    Assert::AreEqual(TERRAIN, PixelAt(pixels, 0, 0), L"visible is the terrain colour");
    // Half of r 0x80, g 0xC0, b 0x40 is r 0x40, g 0x60, b 0x20.
    Assert::AreEqual(0xFF206040u, PixelAt(pixels, 2, 0), L"explored is half as bright");
    Assert::AreEqual(0xFF000000u, PixelAt(pixels, 4, 0), L"and unexplored is black");
  }

  TEST_METHOD(ASmallLandscapeIsTwoPixelsACellAndIsNotResampled)
  {
    // The whole reason §9.2 fixes the panel at 288 and the client area at 256: a Small landscape's
    // 128 cells land on whole pixels, so the map is the grid rather than a filtered copy of it.
    Neuron::FogView fog = Fog(Neuron::FogShade::Unexplored);
    fog.cells[0] = static_cast<std::uint8_t>(Neuron::FogShade::Visible);
    std::vector<std::uint8_t> pixels;
    const std::array<std::uint32_t, 1> palette{RED};
    Neuron::BuildMinimap(ViewOf(fog, {}, palette), pixels);
    Assert::AreEqual(4u, CountOf(pixels, TERRAIN), L"one visible cell is exactly four pixels");
    for (const std::int32_t x : {0, 1})
    {
      for (const std::int32_t y : {0, 1})
      {
        Assert::AreEqual(TERRAIN, PixelAt(pixels, x, y), L"and they are the four in the corner");
      }
    }
  }

  TEST_METHOD(ADeviceIsOnePixelWhereverItStands)
  {
    // One pixel a device (§9.2), and in the right half: a minimap that transposed x and z would
    // send a commander's reinforcements to the other side of the island every time.
    const Neuron::FogView fog = Fog(Neuron::FogShade::Visible);
    const std::array<std::uint32_t, 2> palette{RED, BLUE};
    // Cell (10, 90) is world (640, 5760), which is pixel (20, 180) at two pixels a cell.
    const std::array<Neuron::MinimapBlip, 1> blips{Device(640.0f, 5760.0f, 1)};
    std::vector<std::uint8_t> pixels;
    Neuron::BuildMinimap(ViewOf(fog, blips, palette), pixels);
    Assert::AreEqual(BLUE, PixelAt(pixels, 20, 180), L"the device is where it stands");
    Assert::AreEqual(1u, CountOf(pixels, BLUE), L"and it is one pixel and not a block");
    Assert::AreEqual(TERRAIN, PixelAt(pixels, 180, 20), L"and not at the transpose of it");
  }

  TEST_METHOD(AStructureCoversItsFootprintAndIsCenteredOnIt)
  {
    // §9.2: "structures, two pixels a cell of their footprint". A structure drawn as one pixel is
    // a base a commander cannot find, and one drawn from its corner is a base two cells off.
    const Neuron::FogView fog = Fog(Neuron::FogShade::Visible);
    const std::array<std::uint32_t, 2> palette{RED, BLUE};
    // A three-by-three structure whose middle is world (640, 640): pixel (20, 20), six pixels a side.
    const std::array<Neuron::MinimapBlip, 1> blips{Structure(640.0f, 640.0f, 3, 3, 0)};
    std::vector<std::uint8_t> pixels;
    Neuron::BuildMinimap(ViewOf(fog, blips, palette), pixels);
    Assert::AreEqual(36u, CountOf(pixels, RED), L"three cells a side at two pixels a cell");
    Assert::AreEqual(RED, PixelAt(pixels, 20, 20), L"its middle is where it stands");
    Assert::AreEqual(RED, PixelAt(pixels, 17, 17), L"and it reaches three pixels up and left");
    Assert::AreEqual(TERRAIN, PixelAt(pixels, 16, 16), L"and no further");
  }

  TEST_METHOD(ADeviceIsDrawnOverAStructureAndNotUnderIt)
  {
    // The order §9.2 lists, and it matters where a truck stands on its own factory: a device under
    // a structure's block is a unit the commander cannot find on the map.
    const Neuron::FogView fog = Fog(Neuron::FogShade::Visible);
    const std::array<std::uint32_t, 2> palette{RED, BLUE};
    const std::array<Neuron::MinimapBlip, 2> blips{Device(640.0f, 640.0f, 1), Structure(640.0f, 640.0f, 3, 3, 0)};
    std::vector<std::uint8_t> pixels;
    Neuron::BuildMinimap(ViewOf(fog, blips, palette), pixels);
    Assert::AreEqual(BLUE, PixelAt(pixels, 20, 20), L"the device won the pixel it shares");
  }

  TEST_METHOD(TheSelectionIsDrawnInAccentOverWhateverColorItHad)
  {
    const Neuron::FogView fog = Fog(Neuron::FogShade::Visible);
    const std::array<std::uint32_t, 2> palette{RED, BLUE};
    const std::array<Neuron::MinimapBlip, 2> blips{Device(640.0f, 640.0f, 1, true), Device(704.0f, 640.0f, 1, false)};
    std::vector<std::uint8_t> pixels;
    Neuron::BuildMinimap(ViewOf(fog, blips, palette), pixels);
    Assert::AreEqual(ACCENT, PixelAt(pixels, 20, 20), L"the selected one is accent");
    Assert::AreEqual(BLUE, PixelAt(pixels, 22, 20), L"and the one beside it is still its commander's");
  }

  TEST_METHOD(AGhostStructureIsHalfAsBrightAsAStandingOne)
  {
    // The fog's rule again, applied to a building the commander remembers rather than sees. A ghost
    // at full brightness reads as a live base, which is the one thing the fog exists to prevent.
    const Neuron::FogView fog = Fog(Neuron::FogShade::Explored);
    const std::array<std::uint32_t, 1> palette{0xFF804020u};
    Neuron::MinimapBlip ghost = Structure(640.0f, 640.0f, 1, 1, 0);
    ghost.ghost = true;
    const std::array<Neuron::MinimapBlip, 1> blips{ghost};
    std::vector<std::uint8_t> pixels;
    Neuron::BuildMinimap(ViewOf(fog, blips, palette), pixels);
    Assert::AreEqual(0xFF402010u, PixelAt(pixels, 20, 20), L"half of r 0x20, g 0x40, b 0x80");
  }

  TEST_METHOD(TheCameraQuadrilateralIsDrawnAndIsClippedToTheMap)
  {
    // §9.2's last layer. The corners are where the frustum meets the ground, which on a high camera
    // over a corner of the landscape is mostly off the map - and a line that wrote off the end of
    // the buffer would be a crash rather than a drawing fault.
    const Neuron::FogView fog = Fog(Neuron::FogShade::Visible);
    const std::array<std::uint32_t, 1> palette{RED};
    Neuron::MinimapView view = ViewOf(fog, {}, palette);
    // A square from world (640, 640) to (1280, 1280): pixels (20, 20) to (40, 40).
    view.frustumGround = {640.0f, 640.0f, 1280.0f, 640.0f, 1280.0f, 1280.0f, 640.0f, 1280.0f};
    std::vector<std::uint8_t> pixels;
    Neuron::BuildMinimap(view, pixels);
    Assert::AreEqual(BORDER, PixelAt(pixels, 20, 20), L"a corner");
    Assert::AreEqual(BORDER, PixelAt(pixels, 30, 20), L"the top edge");
    Assert::AreEqual(BORDER, PixelAt(pixels, 40, 30), L"the right edge");
    Assert::AreEqual(TERRAIN, PixelAt(pixels, 30, 30), L"and the middle is not filled");

    // And WHOLLY off one side, which must draw nothing at all rather than wrap round the map or
    // read past the end of the buffer. A camera pointed away from the landscape is an ordinary
    // frame, and a border that reappeared on the far edge would read as a second camera.
    view.frustumGround = {-4000.0f, -4000.0f, -2000.0f, -4000.0f, -2000.0f, -2000.0f, -4000.0f, -2000.0f};
    Neuron::BuildMinimap(view, pixels);
    Assert::AreEqual(std::size_t{Neuron::MINIMAP_BYTES}, pixels.size(), L"the buffer is untouched in size");
    Assert::AreEqual(0u, CountOf(pixels, BORDER), L"and not one border pixel is on the map");
  }

  TEST_METHOD(AFrustumOfAllZeroesDrawsNone)
  {
    // The frame before the join, where there is no camera to draw. A quadrilateral at the origin
    // would put a mark in the corner of the map that means nothing.
    const Neuron::FogView fog = Fog(Neuron::FogShade::Visible);
    const std::array<std::uint32_t, 1> palette{RED};
    std::vector<std::uint8_t> pixels;
    Neuron::BuildMinimap(ViewOf(fog, {}, palette), pixels);
    Assert::AreEqual(0u, CountOf(pixels, BORDER), L"no border pixel anywhere");
  }

  TEST_METHOD(NoFogIsABlackMapRatherThanAFault)
  {
    std::vector<std::uint8_t> pixels;
    Neuron::MinimapView view{};
    view.terrainColor = TERRAIN;
    Neuron::BuildMinimap(view, pixels);
    Assert::AreEqual(std::size_t{Neuron::MINIMAP_BYTES}, pixels.size(), L"still the right size");
    Assert::AreEqual(Neuron::MINIMAP_PIXELS * Neuron::MINIMAP_PIXELS, CountOf(pixels, 0xFF000000u), L"and all black");
  }

  TEST_METHOD(AClickAnswersTheWorldPointUnderIt)
  {
    // §9.2's left click, and the inverse of everything above: the point the camera moves to is the
    // point the commander clicked, through the MIDDLE of the pixel, which is where it was drawn.
    const Neuron::MinimapPoint corner = Neuron::WorldOfMinimap(0, 0, CELLS, WORLD_UNITS_PER_CELL);
    Assert::IsTrue(corner.inside, L"the corner is on the map");
    Assert::AreEqual(16.0f, corner.x, 0.01f, L"half a pixel is a quarter of a cell");
    Assert::AreEqual(16.0f, corner.z, 0.01f, L"in both axes");

    const Neuron::MinimapPoint middle = Neuron::WorldOfMinimap(20, 180, CELLS, WORLD_UNITS_PER_CELL);
    Assert::AreEqual(656.0f, middle.x, 0.01f, L"pixel 20 is world 656");
    Assert::AreEqual(5776.0f, middle.z, 0.01f, L"pixel 180 is world 5776");
  }

  TEST_METHOD(AClickOutsideTheMapIsRefusedRatherThanClamped)
  {
    // A pointer in the panel's border, or in the letterbox. Clamping would move the camera to the
    // edge of the landscape whenever the commander missed the map, which reads as the game
    // dragging him somewhere he did not ask to go.
    Assert::IsFalse(Neuron::WorldOfMinimap(-1, 10, CELLS, WORLD_UNITS_PER_CELL).inside, L"left of it");
    Assert::IsFalse(Neuron::WorldOfMinimap(10, -1, CELLS, WORLD_UNITS_PER_CELL).inside, L"above it");
    Assert::IsFalse(Neuron::WorldOfMinimap(256, 10, CELLS, WORLD_UNITS_PER_CELL).inside, L"right of it");
    Assert::IsFalse(Neuron::WorldOfMinimap(10, 256, CELLS, WORLD_UNITS_PER_CELL).inside, L"below it");
    Assert::IsFalse(Neuron::WorldOfMinimap(10, 10, 0, WORLD_UNITS_PER_CELL).inside, L"and a landscape of no cells");
  }

  TEST_METHOD(ALandscapeBiggerThanTheMapIsOnePixelACell)
  {
    // §11 hands the scale rule for Medium and Large to the milestone that ships them; what this
    // must not do meanwhile is divide by zero or draw a Medium landscape at two pixels a cell and
    // put half of it off the map with no warning.
    Neuron::FogView fog{};
    fog.cellsPerSide = 512;
    fog.cells.assign(static_cast<std::size_t>(512) * 512, static_cast<std::uint8_t>(Neuron::FogShade::Visible));
    const std::array<std::uint32_t, 1> palette{RED};
    Neuron::MinimapView view = ViewOf(fog, {}, palette);
    std::vector<std::uint8_t> pixels;
    Neuron::BuildMinimap(view, pixels);
    Assert::AreEqual(Neuron::MINIMAP_PIXELS * Neuron::MINIMAP_PIXELS, CountOf(pixels, TERRAIN), L"the map is filled at one pixel a cell");
    Assert::AreEqual(1.0f, Neuron::WorldOfMinimap(1, 0, 512, WORLD_UNITS_PER_CELL).x / 96.0f, 0.01f,
                     L"and a click reads back at that scale");
  }
};

} // namespace ClientTests
