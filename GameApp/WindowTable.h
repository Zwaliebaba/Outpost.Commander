#pragma once

namespace Outpost
{
// Where a window first stands, before the player moves it (ADR-100 decision 8), each an anchor of the screen less a margin,
// so that a window not yet moved stands there on any screen and at any step, and never moves as the HUD's panels change: in
// the top-right corner; in one of the places under the Ore that production and research take; at the right edge, level with
// those; in the middle of the screen; or at the bottom right, beside the selection and its buttons, or over the bottom row
// where they leave it no room.
enum class FirstPlace : std::uint8_t
{
  TopRight,
  UnderTheOre,
  RightUnderTheOre,
  Middle,
  BesideTheSelection
};

// A window of the game, as a row of one table (ADR-100): which it is; its name in the interface's file, which is never
// changed (ADR-099 decision 3); its title, on its title bar; the key that opens and closes it, or none; where it first
// stands; whether WindowsFit counts it, so that the interface is scaled only as far as it still fits (ADR-070); and whether
// its place is kept from one session to the next (ADR-099 decision 1).
struct WindowRow
{
  WindowKind kind = WindowKind::Designer;
  std::string_view name;
  std::string_view title;
  std::uint8_t key = 0;
  FirstPlace first = FirstPlace::Middle;
  bool fits = false;
  bool kept = false;
};

// Every window, in WindowKind's order. The designer's title stands in its body, over its name, and not on its bar. Esc opens
// the leave window in a universe, and the away window opens by itself when a report of a time away arrives, so neither has a
// key of its own.
inline constexpr std::array<WindowRow, WINDOW_KINDS> WINDOWS{{
  {.kind = WindowKind::Designer,
   .name = "designer",
   .title = "",
   .key = KEY_DESIGNER,
   .first = FirstPlace::TopRight,
   .fits = true,
   .kept = true},
  {.kind = WindowKind::Research,
   .name = "research",
   .title = "RESEARCH",
   .key = KEY_RESEARCH,
   .first = FirstPlace::UnderTheOre,
   .fits = false,
   .kept = true},
  {.kind = WindowKind::Production,
   .name = "production",
   .title = "PRODUCTION",
   .key = KEY_PRODUCTION,
   .first = FirstPlace::UnderTheOre,
   .fits = false,
   .kept = true},
  {.kind = WindowKind::Controls,
   .name = "controls",
   .title = "CONTROLS",
   .key = KEY_CONTROLS,
   .first = FirstPlace::Middle,
   .fits = true,
   .kept = true},
  {.kind = WindowKind::Orders,
   .name = "orders",
   .title = "SCHEDULED ORDERS",
   .key = KEY_ORDERS,
   .first = FirstPlace::RightUnderTheOre,
   .fits = true,
   .kept = true},
  {.kind = WindowKind::Away,
   .name = "away",
   .title = "WHILE YOU WERE AWAY",
   .key = 0,
   .first = FirstPlace::Middle,
   .fits = true,
   .kept = true},
  {.kind = WindowKind::Leave,
   .name = "leave",
   .title = "LEAVE UNIVERSE",
   .key = 0,
   .first = FirstPlace::Middle,
   .fits = false,
   .kept = true},
  {.kind = WindowKind::Build,
   .name = "build",
   .title = "BUILD",
   .key = KEY_BUILD,
   .first = FirstPlace::BesideTheSelection,
   .fits = true,
   .kept = true},
}};

static_assert(
  []
  {
    for (std::size_t row = 0; row < WINDOWS.size(); ++row)
    {
      if (static_cast<std::size_t>(WINDOWS[row].kind) != row)
        return false;
    }
    return true;
  }(),
  "the table is in WindowKind's order");

// _window's row.
[[nodiscard]] constexpr const WindowRow& RowOf(WindowKind _window) noexcept
{
  return WINDOWS[static_cast<std::size_t>(_window)];
}
} // namespace Outpost
