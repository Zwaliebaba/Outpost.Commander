#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The look of the interface as data (Design/Interface.md §3; TechnicalDesign.md §8): the chrome
// palette every panel, button and bar is drawn in, and the eight commander colours a device wears
// (GameDesign.md §11). One file holds both because the same panels and the same minimap read them,
// and eight rows do not earn a second loader.
//
// THESE ARE 8-BIT RGBA AND NOT HUNDREDTHS, unlike BiomeDesc's renderer numbers. A biome's light is
// multiplied into a surface and needs headroom above one; a chrome colour is written to the
// framebuffer as authored, and a commander colour is drawn unlit by the ruling of GameDesign.md
// §11, so for both the authored byte is the byte that reaches the glass and a hundredth would only
// be a rounding step on the way.

namespace Outpost
{

/// One colour as the interface draws it.
struct Rgba8
{
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
  std::uint8_t alpha;

  [[nodiscard]] constexpr bool operator==(const Rgba8&) const noexcept = default;
};

/// Every colour Design/Interface.md §3 names, under that document's own names, so that a reader
/// with the document open finds each one where they expect it.
struct ChromePalette
{
  Rgba8 panelFill;
  Rgba8 panelBorder;
  Rgba8 panelTitleFrom;
  Rgba8 panelTitleTo;
  Rgba8 titleText;
  Rgba8 bodyText;
  Rgba8 dimText;
  Rgba8 accent;
  Rgba8 warning;
  Rgba8 buttonFill;
  Rgba8 buttonFillHover;
  Rgba8 buttonFillDown;
  Rgba8 buttonDisabled;
  Rgba8 barEmpty;
  Rgba8 barBuild;
  Rgba8 barHealth;
  Rgba8 barHealthLow;

  [[nodiscard]] constexpr bool operator==(const ChromePalette&) const noexcept = default;
};

inline constexpr std::size_t CHROME_ROLE_COUNT = 17;

/// A role's name beside the field it fills. The loader walks this table rather than seventeen
/// hand-written lines, which is what lets it say both "the role 'accent' is missing" and "there is
/// no role 'acccent'" — a palette that silently ignored a misspelled role would leave the panel
/// drawn in whatever the field defaulted to, and nobody would find out until they looked at it.
struct ChromeRole
{
  const char* name;
  Rgba8 ChromePalette::*member;
};

inline constexpr std::array<ChromeRole, CHROME_ROLE_COUNT> CHROME_ROLES = {{
  {"panelFill", &ChromePalette::panelFill},
  {"panelBorder", &ChromePalette::panelBorder},
  {"panelTitleFrom", &ChromePalette::panelTitleFrom},
  {"panelTitleTo", &ChromePalette::panelTitleTo},
  {"titleText", &ChromePalette::titleText},
  {"bodyText", &ChromePalette::bodyText},
  {"dimText", &ChromePalette::dimText},
  {"accent", &ChromePalette::accent},
  {"warning", &ChromePalette::warning},
  {"buttonFill", &ChromePalette::buttonFill},
  {"buttonFillHover", &ChromePalette::buttonFillHover},
  {"buttonFillDown", &ChromePalette::buttonFillDown},
  {"buttonDisabled", &ChromePalette::buttonDisabled},
  {"barEmpty", &ChromePalette::barEmpty},
  {"barBuild", &ChromePalette::barBuild},
  {"barHealth", &ChromePalette::barHealth},
  {"barHealthLow", &ChromePalette::barHealthLow},
}};

/// How many commanders a match holds, and so how many colours this file must carry in seat order.
/// SIM'S MAX_SEATS IS THE AUTHORITY and this repeats it, because Sim reads Content and Content may
/// not read Sim (AGENTS.md R9). Sim/Sim.cpp static_asserts that the two agree; it is the first
/// translation unit where both are visible, so a change to either is caught at the build rather
/// than by a seat drawn in whatever was left in the array.
inline constexpr std::size_t COMMANDER_COLOR_COUNT = 8;

/// How many 32x32 cells Icons.dds carries (Design/Interface.md §3 and §11 row 7). Eight columns of
/// four, which is 256 by 128 - the size the sheet already was, by coincidence, before it held this
/// game's own icons.
inline constexpr std::size_t ICON_CELL_COUNT = 32;

/// WHICH CELL IS WHICH ICON, by name (Design/Interface.md §11 row 7: "the icon list ... with
/// Icons.dds laid out as a grid of 32x32 cells"). It is a table rather than constants because a
/// panel names a STRUCTURE or a MODULE by its content id - "Factory", "MachineGun" - and the cell
/// it wants is therefore looked up rather than written down; the cursors, orders and stances are
/// fixed sets and are named "CursorBuild", "OrderPatrol", "StanceHoldFire" and so on.
///
/// A NAME THE TABLE DOES NOT CARRY ANSWERS NO ICON rather than cell zero. A button with no icon
/// draws its caption and nothing else, which is a panel a commander can still use; a button drawn
/// with the arrow cursor because its name was misspelled is one nobody can read.
inline constexpr std::uint32_t NO_ICON = 0xFFFFFFFFu;

struct IconNames
{
  /// Ascending by name, so that a lookup is a bisection and two names that differ only in case are
  /// two names - the loader refuses a repeat.
  std::vector<std::pair<std::string, std::uint32_t>> cells;

  [[nodiscard]] std::uint32_t Find(std::string_view _name) const noexcept
  {
    const auto at =
      std::lower_bound(cells.begin(), cells.end(), _name,
                       [](const std::pair<std::string, std::uint32_t>& _row, std::string_view _wanted) { return _row.first < _wanted; });
    return at != cells.end() && at->first == _name ? at->second : NO_ICON;
  }

  [[nodiscard]] bool operator==(const IconNames&) const noexcept = default;
};

struct InterfaceDesc
{
  ChromePalette chrome;
  std::array<Rgba8, COMMANDER_COLOR_COUNT> commanders;
  IconNames icons;

  [[nodiscard]] bool operator==(const InterfaceDesc&) const noexcept = default;
};

} // namespace Outpost
