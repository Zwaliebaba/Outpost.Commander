#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
using Outpost::InterfaceSettings;
using Outpost::WindowKind;
using Place = Outpost::InterfaceSettings::Place;
using Point = Outpost::WindowManager::Point;

// How near two corners must be to be the same, in reference units: measuring one from the right edge and back loses no
// more than this.
constexpr float SAME_UNITS = 0.001f;

bool Near(Point _a, Point _b)
{
  return std::abs(_a.xUnits - _b.xUnits) < SAME_UNITS && std::abs(_a.yUnits - _b.yUnits) < SAME_UNITS;
}

std::size_t IndexOf(WindowKind _window)
{
  return static_cast<std::size_t>(_window);
}

// A folder of a test's own under the temporary folder, empty as the test starts and removed after it.
class TestFolder : Neuron::NonCopyable
{
public:
  explicit TestFolder(std::wstring_view _name)
    : m_path(std::filesystem::temp_directory_path() / L"OutpostCommanderTests" / _name)
  {
    (void)std::filesystem::remove_all(m_path);
  }

  ~TestFolder()
  {
    std::error_code ignored;
    (void)std::filesystem::remove_all(m_path, ignored);
  }

  [[nodiscard]] const std::filesystem::path& Path() const noexcept
  {
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

// Settings as a player leaves them: a step up, the designer against the top-right corner and production low on the left.
InterfaceSettings Kept()
{
  InterfaceSettings settings;
  settings.scalePercent = 125;
  settings.places[IndexOf(WindowKind::Designer)] = Place{.fromRight = true, .fromBottom = false, .xUnits = 834.0f, .yUnits = 16.0f};
  settings.places[IndexOf(WindowKind::Production)] = Place{.fromRight = false, .fromBottom = true, .xUnits = 16.0f, .yUnits = 380.5f};
  return settings;
}
} // namespace

TEST_CLASS(InterfaceSettingsTests)
{
public:
  // ADR-099: what is written is read back as it was, through the text and through the file, and the file's folder is made
  // when there is none; nothing is left beside it.
  TEST_METHOD(KeepsTheScaleAndThePlacesThroughTheFile)
  {
    const InterfaceSettings kept = Kept();
    const std::string text = Outpost::WriteInterfaceSettings(kept);
    Assert::IsTrue(text.find(R"("designer": {"from": "topRight", "xUnits": 834, "yUnits": 16})") != std::string::npos, L"a place reads");
    Assert::IsTrue(Outpost::ReadInterfaceSettings(text) == kept, L"through the text");
    Assert::IsTrue(Outpost::ReadInterfaceSettings(Outpost::WriteInterfaceSettings(InterfaceSettings{})) == InterfaceSettings{},
                   L"no window moved");

    const TestFolder folder(L"KeepsTheScaleAndThePlacesThroughTheFile");
    const std::filesystem::path file = folder.Path() / L"Outpost Commander" / L"Interface.json";
    Assert::IsTrue(Outpost::SaveInterfaceSettings(kept, file));
    Assert::IsTrue(Outpost::LoadInterfaceSettings(file) == kept, L"through the file");
    Assert::IsTrue(std::distance(std::filesystem::directory_iterator(file.parent_path()), std::filesystem::directory_iterator()) == 1,
                   L"the file alone");

    InterfaceSettings stepped = kept;
    stepped.scalePercent = 150;
    Assert::IsTrue(Outpost::SaveInterfaceSettings(stepped, file));
    Assert::IsTrue(Outpost::LoadInterfaceSettings(file) == stepped, L"written again over the last");
  }

  // ADR-099: the game starts as it would without the file when there is none, and when what it holds is not settings of this
  // version: empty, cut short, not an object, another file of the game's, without a version, or of another.
  TEST_METHOD(StartsWithoutAFileItCannotRead)
  {
    const TestFolder folder(L"StartsWithoutAFileItCannotRead");
    Assert::IsFalse(Outpost::LoadInterfaceSettings(folder.Path() / L"Interface.json").has_value(), L"no file");

    for (const std::string_view text :
         {std::string_view(""), std::string_view(R"({"version": 1, "scalePercent": 125)"), std::string_view("[1, 2]"),
          std::string_view(R"({"host": "203.0.113.5", "port": 4433})"), std::string_view(R"({"scalePercent": 125})"),
          std::string_view(R"({"version": 2, "scalePercent": 125})"), std::string_view(R"({"version": "1"})")})
      Assert::IsFalse(Outpost::ReadInterfaceSettings(text).has_value(), std::wstring(text.begin(), text.end()).c_str());

    (void)std::filesystem::create_directories(folder.Path());
    const std::filesystem::path file = folder.Path() / L"Interface.json";
    std::ofstream(file, std::ios::binary) << "not settings";
    Assert::IsFalse(Outpost::LoadInterfaceSettings(file).has_value(), L"a file of something else");
  }

  // ADR-099: a member that is not as the game writes it is left out, and so is a window this build does not have, while the
  // rest is read: a scale that is no step, a corner with no name, a distance that is no number or past any screen.
  TEST_METHOD(LeavesOutWhatItDoesNotKnow)
  {
    const std::optional<InterfaceSettings> read = Outpost::ReadInterfaceSettings(R"({
  "version": 1,
  "scalePercent": 130,
  "windows": {
    "build": {"from": "topLeft", "xUnits": 10, "yUnits": 10},
    "designer": {"from": "middle", "xUnits": 1, "yUnits": 1},
    "research": {"from": "topLeft", "xUnits": "16", "yUnits": 1},
    "away": {"from": "topLeft", "xUnits": 1e9, "yUnits": 0},
    "orders": {"from": "bottomRight", "xUnits": 600, "yUnits": 700.25}
  }
})");
    Assert::IsTrue(read.has_value());
    const InterfaceSettings settings = read.value_or(InterfaceSettings{});
    Assert::AreEqual(100, settings.scalePercent, L"130% is no step");
    Assert::IsFalse(settings.places[IndexOf(WindowKind::Designer)].has_value(), L"no such corner");
    Assert::IsFalse(settings.places[IndexOf(WindowKind::Research)].has_value(), L"a distance as text");
    Assert::IsFalse(settings.places[IndexOf(WindowKind::Away)].has_value(), L"past any screen");
    Assert::IsTrue(settings.places[IndexOf(WindowKind::Orders)] ==
                   Place{.fromRight = true, .fromBottom = true, .xUnits = 600.0f, .yUnits = 700.25f});
    const auto kept = std::ranges::count_if(settings.places, [](const std::optional<Place>& _place) { return _place.has_value(); });
    Assert::IsTrue(kept == 1, L"the window this build does not have is left out");
  }

  // ADR-099: a window is kept from the screen's corner nearest its middle, so that one left against the right edge stays
  // against it on a wider screen and a narrower one, and one low on the left stays as far above the foot.
  TEST_METHOD(KeepsAWindowFromTheCornerNearestIt)
  {
    // The designer, 818 by 810 units, against the top-right corner of the reference screen.
    const Place designer = Outpost::PlaceOf({.xUnits = 1086.0f, .yUnits = 16.0f}, 818.0f, 810.0f, 1920.0f, 1080.0f);
    Assert::IsTrue(designer == Place{.fromRight = true, .fromBottom = false, .xUnits = 834.0f, .yUnits = 16.0f});
    Assert::IsTrue(Near(Outpost::CornerOf(designer, 1920.0f, 1080.0f), {.xUnits = 1086.0f, .yUnits = 16.0f}), L"back where it was");
    const Point wider = Outpost::CornerOf(designer, 2560.0f, 1080.0f);
    Assert::IsTrue(Near(wider, {.xUnits = 1726.0f, .yUnits = 16.0f}));
    Assert::AreEqual(16.0f, 2560.0f - (wider.xUnits + 818.0f), SAME_UNITS, L"its right edge as far from the screen's");
    Assert::IsTrue(Near(Outpost::CornerOf(designer, 1536.0f, 864.0f), {.xUnits = 702.0f, .yUnits = 16.0f}), L"at 125%");

    // Production, 478 by 300 units, low on the left.
    const Place production = Outpost::PlaceOf({.xUnits = 16.0f, .yUnits = 700.0f}, 478.0f, 300.0f, 1920.0f, 1080.0f);
    Assert::IsTrue(production == Place{.fromRight = false, .fromBottom = true, .xUnits = 16.0f, .yUnits = 380.0f});
    Assert::IsTrue(Near(Outpost::CornerOf(production, 1536.0f, 864.0f), {.xUnits = 16.0f, .yUnits = 484.0f}));
  }

  // ADR-099: a kept place that lies off a smaller screen is brought back by the clamp every window has (ADR-031 decision 2):
  // its title bar on the screen, and 120 units of its width.
  TEST_METHOD(ClampsAKeptPlaceOnASmallerScreen)
  {
    const Place offScreen{.fromRight = true, .fromBottom = true, .xUnits = 2000.0f, .yUnits = 2000.0f};
    const Point corner = Outpost::CornerOf(offScreen, 1280.0f, 720.0f);
    const Point kept = Outpost::Hud::KeepOnScreen(corner, 818.0f, 1280.0f, 720.0f);
    Assert::AreEqual(Outpost::Hud::WINDOW_KEPT_ON_SCREEN_UNITS - 818.0f, kept.xUnits, SAME_UNITS, L"120 units of it on the screen");
    Assert::AreEqual(0.0f, kept.yUnits, SAME_UNITS, L"its title bar on the screen");
  }

  // ADR-099: the scale is kept as the percentage of its step, and a percentage that names no step is 100%.
  TEST_METHOD(KeepsTheScaleAsItsStep)
  {
    for (const float step : Outpost::Hud::INTERFACE_STEPS)
      Assert::AreEqual(step, Outpost::ScaleFactorOf(Outpost::ScalePercentOf(step)));
    Assert::AreEqual(110, Outpost::ScalePercentOf(1.1f));
    Assert::AreEqual(175, Outpost::ScalePercentOf(1.75f));
    Assert::AreEqual(1.0f, Outpost::ScaleFactorOf(130));
  }
};
} // namespace GameAppTests
