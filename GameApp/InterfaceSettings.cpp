#include "pch.h"
#include "InterfaceSettings.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

namespace
{
using Outpost::InterfaceSettings;

// The windows' names in the file, in WindowKind's order. A name is never changed, since a file an earlier build wrote names
// its windows so.
constexpr std::array<std::string_view, Outpost::WINDOW_KINDS> WINDOW_NAMES{"designer", "research", "production", "controls",
                                                                           "orders",   "away",     "leave",      "build"};
static_assert(static_cast<std::size_t>(Outpost::WindowKind::Build) + 1 == Outpost::WINDOW_KINDS, "a name for every window");

// The corner a place is measured from, by its fromBottom and then its fromRight.
constexpr std::array<std::string_view, 4> CORNER_NAMES{"topLeft", "topRight", "bottomLeft", "bottomRight"};

// No screen is this many reference units across, so a place further than this from its corner is not read.
constexpr double MOST_UNITS = 100'000.0;

std::size_t CornerIndex(const InterfaceSettings::Place& _place) noexcept
{
  return (_place.fromBottom ? 2U : 0U) + (_place.fromRight ? 1U : 0U);
}

// A distance in reference units as the file holds it: a number no further than MOST_UNITS from its corner.
std::optional<float> UnitsIn(const Neuron::JsonValue* _value)
{
  if (_value == nullptr || !_value->IsNumber() || std::abs(_value->AsNumber()) > MOST_UNITS)
    return std::nullopt;
  return static_cast<float>(_value->AsNumber());
}

// A window's place as the file holds it, or none when it is not one.
std::optional<InterfaceSettings::Place> PlaceIn(const Neuron::JsonValue& _value)
{
  if (!_value.IsObject())
    return std::nullopt;
  const Neuron::JsonValue* from = _value.Find("from");
  if (from == nullptr || !from->IsString())
    return std::nullopt;
  const auto corner = std::ranges::find(CORNER_NAMES, std::string_view(from->AsString()));
  const std::optional<float> x = UnitsIn(_value.Find("xUnits"));
  const std::optional<float> y = UnitsIn(_value.Find("yUnits"));
  if (corner == CORNER_NAMES.end() || !x.has_value() || !y.has_value())
    return std::nullopt;
  const auto index = static_cast<std::size_t>(corner - CORNER_NAMES.begin());
  return InterfaceSettings::Place{.fromRight = (index & 1U) != 0, .fromBottom = (index & 2U) != 0, .xUnits = *x, .yUnits = *y};
}
} // namespace

Outpost::InterfaceSettings::Place Outpost::PlaceOf(WindowManager::Point _corner, float _widthUnits, float _heightUnits,
                                                   float _screenWidthUnits, float _screenHeightUnits) noexcept
{
  const bool right = _corner.xUnits + (_widthUnits / 2.0f) > _screenWidthUnits / 2.0f;
  const bool bottom = _corner.yUnits + (_heightUnits / 2.0f) > _screenHeightUnits / 2.0f;
  return {.fromRight = right,
          .fromBottom = bottom,
          .xUnits = right ? _screenWidthUnits - _corner.xUnits : _corner.xUnits,
          .yUnits = bottom ? _screenHeightUnits - _corner.yUnits : _corner.yUnits};
}

Outpost::WindowManager::Point Outpost::CornerOf(const InterfaceSettings::Place& _place, float _screenWidthUnits,
                                                float _screenHeightUnits) noexcept
{
  return {.xUnits = _place.fromRight ? _screenWidthUnits - _place.xUnits : _place.xUnits,
          .yUnits = _place.fromBottom ? _screenHeightUnits - _place.yUnits : _place.yUnits};
}

std::int32_t Outpost::ScalePercentOf(float _factor) noexcept
{
  return static_cast<std::int32_t>(std::lround(_factor * 100.0f));
}

float Outpost::ScaleFactorOf(std::int32_t _percent) noexcept
{
  const auto step = std::ranges::find_if(Hud::INTERFACE_STEPS, [_percent](float _step) { return ScalePercentOf(_step) == _percent; });
  return step != Hud::INTERFACE_STEPS.end() ? *step : Hud::INTERFACE_STEPS.front();
}

std::optional<Outpost::InterfaceSettings> Outpost::ReadInterfaceSettings(std::string_view _text)
{
  Neuron::JsonValue document;
  try
  {
    document = Neuron::ParseJson(_text);
  }
  catch (const Neuron::Exception&)
  {
    return std::nullopt;
  }
  if (!document.IsObject())
    return std::nullopt;
  const Neuron::JsonValue* version = document.Find("version");
  if (version == nullptr || !version->IsNumber() || version->AsNumber() != static_cast<double>(InterfaceSettings::VERSION))
    return std::nullopt;

  InterfaceSettings settings;
  if (const Neuron::JsonValue* scale = document.Find("scalePercent"); scale != nullptr && scale->IsNumber())
  {
    const double percent = scale->AsNumber();
    if (std::ranges::any_of(Hud::INTERFACE_STEPS, [percent](float _step) { return static_cast<double>(ScalePercentOf(_step)) == percent; }))
      settings.scalePercent = static_cast<std::int32_t>(percent);
  }
  if (const Neuron::JsonValue* windows = document.Find("windows"); windows != nullptr && windows->IsObject())
  {
    for (const Neuron::JsonMember& member : windows->AsObject())
    {
      const auto named = std::ranges::find(WINDOW_NAMES, std::string_view(member.name));
      if (named == WINDOW_NAMES.end())
        continue;
      if (const std::optional<InterfaceSettings::Place> place = PlaceIn(member.value); place.has_value())
        settings.places[static_cast<std::size_t>(named - WINDOW_NAMES.begin())] = place;
    }
  }
  return settings;
}

std::string Outpost::WriteInterfaceSettings(const InterfaceSettings& _settings)
{
  std::string text =
    std::format("{{\n  \"version\": {},\n  \"scalePercent\": {},\n  \"windows\": {{", InterfaceSettings::VERSION, _settings.scalePercent);
  bool first = true;
  for (std::size_t window = 0; window < WINDOW_KINDS; ++window)
  {
    const std::optional<InterfaceSettings::Place>& place = _settings.places[window];
    if (!place.has_value())
      continue;
    text +=
      std::format("{}\n    {}: {{\"from\": {}, \"xUnits\": {}, \"yUnits\": {}}}", first ? "" : ",", Neuron::QuoteJson(WINDOW_NAMES[window]),
                  Neuron::QuoteJson(CORNER_NAMES[CornerIndex(*place)]), place->xUnits, place->yUnits);
    first = false;
  }
  text += first ? "}\n}\n" : "\n  }\n}\n";
  return text;
}

std::optional<Outpost::InterfaceSettings> Outpost::LoadInterfaceSettings(const std::filesystem::path& _file) noexcept
{
  try
  {
    std::error_code missing;
    if (!std::filesystem::exists(_file, missing))
      return std::nullopt;
    std::ifstream file(_file, std::ios::binary);
    if (!file)
    {
      Neuron::DebugTrace("The interface's settings in {} cannot be read; it starts as it would without them.\n", _file.string());
      return std::nullopt;
    }
    const std::string text(std::istreambuf_iterator<char>(file), {});
    std::optional<InterfaceSettings> settings = ReadInterfaceSettings(text);
    if (!settings.has_value())
      Neuron::DebugTrace("{} holds no interface settings of version {}; the interface starts as it would without them.\n", _file.string(),
                         InterfaceSettings::VERSION);
    return settings;
  }
  catch (const std::exception&)
  {
    // The file system or memory failed: the game starts as it would without the file.
    return std::nullopt;
  }
}

bool Outpost::SaveInterfaceSettings(const InterfaceSettings& _settings, const std::filesystem::path& _file) noexcept
{
  try
  {
    // A folder that cannot be made fails the write below, which says so.
    std::error_code error;
    (void)std::filesystem::create_directories(_file.parent_path(), error);
    std::filesystem::path written = _file;
    written += L".new";
    {
      std::ofstream file(written, std::ios::binary | std::ios::trunc);
      file << WriteInterfaceSettings(_settings);
      file.close();
      if (!file)
      {
        Neuron::DebugTrace("The interface's settings cannot be written to {}.\n", written.string());
        return false;
      }
    }
    std::filesystem::rename(written, _file, error);
    if (error)
    {
      Neuron::DebugTrace("The interface's settings cannot replace {}: {}\n", _file.string(), error.message());
      return false;
    }
    return true;
  }
  catch (const std::exception&)
  {
    // The file system or memory failed: the settings are written again when they next change, or as the game closes.
    return false;
  }
}
