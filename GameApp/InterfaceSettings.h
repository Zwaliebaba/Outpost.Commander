#pragma once

namespace Outpost
{
// What of the interface lasts from one session to the next (ADR-099): its scale, and where the player left each window it
// moved, measured from the corner of the screen nearest the window, so that a window left against an edge stays against
// it on a screen of another size. A window the player has not moved is not kept, and opens where the HUD puts it.
struct InterfaceSettings
{
  // Where a window's top-left corner stands, in reference units: from the screen's left or right edge, and from its top
  // or bottom edge, whichever the window's middle stood nearer.
  struct Place
  {
    bool fromRight = false;
    bool fromBottom = false;
    float xUnits = 0.0f;
    float yUnits = 0.0f;

    friend bool operator==(const Place&, const Place&) = default;
  };

  // The file's layout. A file of another version is not read.
  static constexpr std::int32_t VERSION = 1;

  // The interface's scale, one of Hud::INTERFACE_STEPS as a percentage (ADR-070).
  std::int32_t scalePercent = 100;
  // In WindowKind's order.
  std::array<std::optional<Place>, WINDOW_KINDS> places{};

  friend bool operator==(const InterfaceSettings&, const InterfaceSettings&) = default;
};

// The place of a window _widthUnits by _heightUnits whose top-left corner stands at _corner, on a screen of this size in
// reference units, from the screen's corner nearest the window's middle.
[[nodiscard]] InterfaceSettings::Place PlaceOf(WindowManager::Point _corner, float _widthUnits, float _heightUnits, float _screenWidthUnits,
                                               float _screenHeightUnits) noexcept;

// Where a window kept at _place stands on a screen of this size: its top-left corner, in reference units. The HUD keeps it
// on the screen as it keeps any window (Hud::KeepOnScreen).
[[nodiscard]] WindowManager::Point CornerOf(const InterfaceSettings::Place& _place, float _screenWidthUnits,
                                            float _screenHeightUnits) noexcept;

// _factor, one of Hud::INTERFACE_STEPS, as a percentage, and back: a percentage that names no step is 100%'s factor.
[[nodiscard]] std::int32_t ScalePercentOf(float _factor) noexcept;
[[nodiscard]] float ScaleFactorOf(std::int32_t _percent) noexcept;

// The settings _text holds, or none when it is not JSON, not an object, or names no version or another one. A member
// that is not as WriteInterfaceSettings writes it is left out, and so is a window this build does not have, so that a
// file a later build wrote still gives what this one knows.
[[nodiscard]] std::optional<InterfaceSettings> ReadInterfaceSettings(std::string_view _text);

// _settings as JSON, as ReadInterfaceSettings reads it.
[[nodiscard]] std::string WriteInterfaceSettings(const InterfaceSettings& _settings);

// The settings in _file: none when it is not there, and none, said to the debugger in Debug, when it cannot be read or
// holds no settings ReadInterfaceSettings reads. It never throws: the game starts as it would without the file.
[[nodiscard]] std::optional<InterfaceSettings> LoadInterfaceSettings(const std::filesystem::path& _file) noexcept;

// Writes _settings to _file, making its folder when there is none, through a file beside it that then replaces it, so that
// a crash never leaves half a file. False, said to the debugger in Debug, when it cannot.
bool SaveInterfaceSettings(const InterfaceSettings& _settings, const std::filesystem::path& _file) noexcept;
} // namespace Outpost
