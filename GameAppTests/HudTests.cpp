#include "pch.h"

#include <algorithm>
#include <array>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId PLAYER{1};
constexpr Outpost::DesignId SWARM{1};
constexpr Outpost::DesignId LINE{2};

// The HUD's fonts as the game rasterizes them for a back buffer of this size, kept for the run, since a TextMetrics reads
// them where they are (ADR-061). A font that is not installed throws, as ADR-030 has it: the tests never guess a width.
std::span<const Neuron::GlyphAtlas::Font> FontsFor(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  static std::map<float, std::vector<Neuron::GlyphAtlas::Font>> g_fontsByScale;
  const float scale = Outpost::Hud::Scale(_widthPixels, _heightPixels);
  auto found = g_fontsByScale.find(scale);
  if (found == g_fontsByScale.end())
  {
    found =
      g_fontsByScale.emplace(scale, Neuron::RasterizeUiAtlas(Outpost::Hud::Typefaces(), Outpost::Hud::Sprites(), scale).glyphs.fonts).first;
  }
  return found->second;
}

Outpost::Hud::TextMetrics MetricsFor(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  return {FontsFor(_widthPixels, _heightPixels), Outpost::Hud::Scale(_widthPixels, _heightPixels)};
}

// The HUD and the menu laid out as GameClient lays them out, with the fonts at the back buffer's scale.
Outpost::Hud::Layout Lay(const Outpost::Hud::Content& _content, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                         std::span<const Outpost::PlanePosition> _view = {}, const Outpost::WindowManager* _windows = nullptr)
{
  return Outpost::Hud::Lay(_content, MetricsFor(_widthPixels, _heightPixels), _widthPixels, _heightPixels, _view, _windows);
}

Outpost::Hud::Layout LayMenu(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  return Outpost::Hud::LayMenu(MetricsFor(_widthPixels, _heightPixels), _widthPixels, _heightPixels);
}

Outpost::Snapshot Newest()
{
  Outpost::Snapshot snapshot{.tick = 5, .player = PLAYER, .ore = 12345};
  snapshot.designs = {{.id = SWARM, .nameUtf8 = "Small+Ion+Mass Driver"}, {.id = LINE, .nameUtf8 = "Medium+Ion+Lance"}};
  return snapshot;
}

// A designer with a name and one card, and a button of the HUD's, for laying windows out.
Outpost::Hud::Content WithDesigner()
{
  Outpost::Hud::Content content;
  content.buttons = {{.label = "Shipyard|300", .action = {.kind = Outpost::Hud::ActionKind::Build}}};
  Outpost::Hud::DesignerPanel designer{.name = "Swarm"};
  designer.slots[0].cards = {{.name = "Small", .action = {.kind = Outpost::Hud::ActionKind::PickHull, .hull = Outpost::HullId{1}}}};
  content.designer = designer;
  return content;
}

// The mockup's designer (Phase 1 design §11): the tuning data's components, with the Large hull, the Fusion drive and the
// Missile Rack locked until _unlocked, and with _fiveWeapons the two of Phase 1 as well; the topics that unlock them; one
// saved design; and Shipyard 01, which has built four ships and has two in its queue.
Outpost::Snapshot DesignerSnapshot(bool _unlocked = false, bool _fiveWeapons = false)
{
  Outpost::Snapshot snapshot{.tick = 5, .player = PLAYER, .ore = 500};
  snapshot.shipyardBuildSpeedFactor = 1.25;
  snapshot.entities = {{.id = Outpost::EntityId{30},
                        .kind = Outpost::EntityKind::Structure,
                        .owner = PLAYER,
                        .structure = Outpost::StructureKind::Shipyard,
                        .shipyardNumber = 1,
                        .shipsBuilt = 4,
                        .queue = {{.design = SWARM}, {.design = SWARM}}}};
  snapshot.hulls = {{Outpost::HullId{1}, "Small", 22000, 200, 60.0, 180.0, 8.0, 32, 10.0, true},
                    {Outpost::HullId{2}, "Medium", 50000, 800, 40.0, 120.0, 14.0, 110, 20.0, true},
                    {Outpost::HullId{3}, "Large", 120000, 1400, 25.0, 60.0, 24.0, 300, 40.0, _unlocked}};
  snapshot.drives = {{Outpost::DriveId{1}, "Ion", 1.3, 0.9, 1.25, 20, true}, {Outpost::DriveId{2}, "Fusion", 0.8, 1.4, 0.8, 80, _unlocked}};
  snapshot.weapons = {{Outpost::WeaponId{1}, "Mass Driver", 1400, 0.4, 120.0, 0.0, 35, true},
                      {Outpost::WeaponId{2}, "Lance", 9500, 2.7, 220.0, 0.0, 85, true},
                      {Outpost::WeaponId{3}, "Missile Rack", 3000, 2.0, 280.0, 30.0, 130, _unlocked}};
  if (_fiveWeapons)
  {
    snapshot.weapons.push_back({Outpost::WeaponId{4}, "Flak Battery", 800, 0.5, 150.0, 20.0, 90, _unlocked});
    snapshot.weapons.push_back({Outpost::WeaponId{5}, "Rail Cannon", 20000, 4.0, 340.0, 0.0, 220, _unlocked});
  }
  snapshot.research = {{.id = Outpost::ResearchTopicId{5}, .nameUtf8 = "Fusion Drive", .unlocksDrive = Outpost::DriveId{2}},
                       {.id = Outpost::ResearchTopicId{6}, .nameUtf8 = "Large Hull", .unlocksHull = Outpost::HullId{3}},
                       {.id = Outpost::ResearchTopicId{7}, .nameUtf8 = "Missile Rack", .unlocksWeapon = Outpost::WeaponId{3}}};
  snapshot.designs = {{.id = SWARM,
                       .nameUtf8 = "Swarm",
                       .hull = Outpost::HullId{1},
                       .drive = Outpost::DriveId{1},
                       .weapon = Outpost::WeaponId{1},
                       .cost = 87}};
  return snapshot;
}

// The designer's panel for _designer, with _hovered under the pointer.
Outpost::Hud::DesignerPanel DesignerOf(const Outpost::Snapshot& _newest, const Outpost::Designer& _designer,
                                       std::optional<Outpost::Hud::Action> _hovered = std::nullopt)
{
  return Outpost::Hud::Describe(_newest, _newest.entities, {}, std::nullopt, &_designer, _hovered)
    .designer.value_or(Outpost::Hud::DesignerPanel{});
}

Outpost::EntityView Ship(std::uint32_t _id, Outpost::DesignId _design, std::int32_t _hitPointsHundredths, std::int32_t _maxHundredths)
{
  return {.id = Outpost::EntityId{_id},
          .owner = PLAYER,
          .design = _design,
          .hitPointsHundredths = _hitPointsHundredths,
          .maxHitPointsHundredths = _maxHundredths};
}
// A name of DESIGN_NAME_LIMIT characters in the name face's widest capital, ending in _last so that names differ.
std::string LongestName(char _last)
{
  return std::string(Outpost::DESIGN_NAME_LIMIT - 1, 'W') + _last;
}

// The longest content the game makes, with every window open (task 14.1): six designs of the longest names, all selected,
// one of them loaded in the designer of five weapons, every part locked or every part unlocked; a full queue at a
// Shipyard and at the Lab; a page of topics, among them Relay Archives with both its prerequisites to do; a placement's
// hint, the research line, the alerts, the territory and the banner. The Ore is five figures, more than any match in the
// review banked.
Outpost::Hud::Content LongestContent(bool _unlocked)
{
  Outpost::Snapshot newest = DesignerSnapshot(_unlocked, true);
  newest.ore = 99999;
  std::vector<Outpost::EntityId> selected;
  for (std::uint32_t i = 0; i < 6; ++i)
  {
    const Outpost::DesignId design{10 + i};
    newest.designs.push_back({.id = design,
                              .nameUtf8 = LongestName(static_cast<char>('A' + i)),
                              .hull = Outpost::HullId{2},
                              .drive = Outpost::DriveId{1},
                              .weapon = Outpost::WeaponId{2},
                              .cost = 1234});
    newest.entities.push_back(Ship(100 + i, design, 150000, 220000));
    selected.push_back(Outpost::EntityId{100 + i});
  }
  Outpost::Designer designer;
  designer.Update(newest);
  designer.Load(newest.designs.back());
  Outpost::Hud::Content content =
    Outpost::Hud::Describe(newest, newest.entities, selected, Outpost::StructureKind::Shipyard, &designer, std::nullopt);
  content.research = "Researching Mass Driver Calibration, 99%";
  content.alerts = {{"Shipyard 05 is under attack", {}}, {"Mining Rig 12 destroyed", {}}, {"Relay 03 lost", {}}};
  content.territory = Outpost::Hud::Territory{.ownNodes = 12, .enemyNodes = 12, .nodes = 12, .ownTickets = 10000, .enemyTickets = 10000};
  content.outcome = Outpost::Hud::Outcome{.title = "Defeat", .detail = "Command Station destroyed \xC2\xB7 Match length 1:02:03"};
  content.buttons = {{.label = "Shipyard|300"},
                     {.label = "Research Lab|400", .enabled = false, .note = "ONE PER PLAYER"},
                     {.label = "Production"},
                     {.label = "Ship designer"},
                     {.label = "Research"}};

  Outpost::Hud::ProductionPanel production{.producer = "SHIPYARD 05", .hasProducer = true, .canStep = true, .ore = newest.ore};
  for (const Outpost::DesignView& design : newest.designs)
    production.options.push_back({.name = design.nameUtf8, .detail = "M\xC2\xB7I\xC2\xB7L", .cost = design.cost});
  for (std::size_t job = 0; job < Outpost::QUEUE_LIMIT; ++job)
    production.queue.push_back({.name = LongestName('Q'), .front = job == 0, .permille = 1000});
  content.production = production;

  Outpost::Hud::ResearchPanel laboratory{.lab = "RESEARCH LAB", .hasLab = true, .ore = newest.ore};
  laboratory.topics = {
    {.name = "Relay Archives", .effect = "Opens tier 2", .cost = 400, .needs = {"IMPROVED EXTRACTION", "HULL PLATING"}},
    {.name = "Mass Driver Calibration", .effect = "Mass Driver fire rate +10%", .cost = 150, .time = "TIER 1 \xC2\xB7 60 s"},
    {.name = "Defense Autoloader",
     .effect = "Defense Gun fire rate +20%",
     .cost = 250,
     .needs = {"MASS DRIVER CALIBRATION", "AUTOMATED SHIPYARDS"}},
    {.name = "Reinforced Structures", .effect = "Every structure's hit points +25%", .cost = 250, .time = "TIER 2 \xC2\xB7 120 s"}};
  while (laboratory.topics.size() < Outpost::Hud::TOPICS_SHOWN + 1)
    laboratory.topics.push_back(
      {.name = "Deep Core Survey", .effect = "Asteroids' Ore reserve +30%", .cost = 300, .time = "TIER 2 \xC2\xB7 90 s"});
  for (std::size_t job = 0; job < Outpost::QUEUE_LIMIT; ++job)
    laboratory.queue.push_back({.name = "Mass Driver Calibration", .front = job == 0, .waiting = job == 0});
  content.laboratory = laboratory;
  return content;
}

// What 14.1's tests lay out: the menu, and the longest content with its parts locked and unlocked.
std::vector<std::pair<std::wstring, Outpost::Hud::Layout>> LongestLayouts(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  std::vector<std::pair<std::wstring, Outpost::Hud::Layout>> layouts;
  layouts.emplace_back(L"the menu", LayMenu(_widthPixels, _heightPixels));
  layouts.emplace_back(L"locked", Lay(LongestContent(false), _widthPixels, _heightPixels));
  layouts.emplace_back(L"unlocked", Lay(LongestContent(true), _widthPixels, _heightPixels));
  return layouts;
}

// The box a text's line is set in, in pixels, as UiPipeline::DrawText sets it: from its pen, rounded to a pixel, as wide as
// its characters' advances and its tracking, and a line of its font tall.
Outpost::Hud::Rect LineBoxOf(const Outpost::Hud::Text& _text, std::span<const Neuron::GlyphAtlas::Font> _fonts)
{
  const Neuron::GlyphAtlas::Font& font = _fonts[static_cast<std::size_t>(_text.typeface)];
  return {.left = std::round(_text.left),
          .top = std::round(_text.top),
          .width = font.Width(_text.text, std::round(_text.trackingPixels)),
          .height = font.lineHeight};
}

// A text named for a failure: its layout, the back buffer's width and what it says.
std::wstring Named(const std::wstring& _layout, std::uint32_t _widthPixels, const Outpost::Hud::Text& _text)
{
  return std::format(L"{} at {} wide: \"{}\"", _layout, _widthPixels, std::wstring(winrt::to_hstring(_text.text)));
}
} // namespace

TEST_CLASS(HudTests)
{
public:
  // ADR-030: the HUD names a font for every typeface and a sprite for every sprite, in their order. The default face is
  // Segoe UI at the text's size; the figures take Cascadia Mono, or Consolas where it is not installed (gate H7).
  TEST_METHOD(NamesItsTypefacesAndSprites)
  {
    const std::vector<Neuron::FontDesc> typefaces = Outpost::Hud::Typefaces();
    Assert::AreEqual(static_cast<std::size_t>(Outpost::Hud::Typeface::Detail) + 1, typefaces.size());
    const Neuron::FontDesc& body = typefaces[static_cast<std::size_t>(Outpost::Hud::Typeface::Body)];
    Assert::IsTrue(body.families == std::vector<std::wstring>{L"Segoe UI"});
    Assert::AreEqual(Outpost::Hud::FONT_UNITS, body.emUnits);
    const Neuron::FontDesc& figure = typefaces[static_cast<std::size_t>(Outpost::Hud::Typeface::Figure)];
    Assert::IsTrue(figure.families == std::vector<std::wstring>{L"Cascadia Mono", L"Consolas"});
    for (const Neuron::FontDesc& typeface : typefaces)
      Assert::IsTrue(!typeface.families.empty() && typeface.emUnits > 0.0f);
    Assert::AreEqual(static_cast<std::size_t>(Outpost::Hud::Sprite::Corner) + 1, Outpost::Hud::Sprites().size());
  }

  // ADR-031: the designer is a window: at first in the top-right corner, its title bar left of its close box, a bracket at
  // each corner, and its buttons its own layer's.
  TEST_METHOD(LaysTheDesignerOutAsAWindow)
  {
    Outpost::WindowManager windows;
    Assert::IsTrue(Lay(WithDesigner(), 1920, 1080, {}, &windows).windows.empty(), L"closed, it is not laid out");
    windows.Open(Outpost::WindowKind::Designer);
    const Outpost::Hud::Layout layout = Lay(WithDesigner(), 1920, 1080, {}, &windows);
    Assert::AreEqual(size_t{1}, layout.windows.size());
    const Outpost::Hud::Window& window = layout.windows.front();
    Assert::IsTrue(window.kind == Outpost::WindowKind::Designer);
    Assert::IsTrue(window.frame.left > 1920.0f - 760.0f && window.frame.top < 30.0f, L"top right");
    Assert::IsTrue(window.titleBar.Contains(window.frame.left + 10.0f, window.frame.top + 10.0f));
    Assert::IsTrue(window.closeBox.left >= window.titleBar.left + window.titleBar.width);
    Assert::IsTrue(window.titleBar.fill == Outpost::Hud::Fill::Hatched);
    const Outpost::Hud::Span sprites = layout.SpritesOf(1);
    const auto corners = std::ranges::count(layout.sprites.begin() + static_cast<std::ptrdiff_t>(sprites.first),
                                            layout.sprites.begin() + static_cast<std::ptrdiff_t>(sprites.end), Outpost::Hud::Sprite::Corner,
                                            &Outpost::Hud::SpriteMark::sprite);
    Assert::AreEqual(std::ptrdiff_t{4}, corners);
    const Outpost::Hud::Span hud = layout.SpritesOf(0);
    Assert::IsTrue(std::all_of(layout.sprites.begin() + static_cast<std::ptrdiff_t>(hud.first),
                               layout.sprites.begin() + static_cast<std::ptrdiff_t>(hud.end),
                               [&window](const Outpost::Hud::SpriteMark& _mark)
                               { return !window.frame.Contains(_mark.area.left, _mark.area.top); }),
                   L"the HUD's sprites are its own panels'");

    const Outpost::Hud::Span actions = layout.ActionsOf(1);
    const auto name = std::ranges::find(layout.actions.begin() + static_cast<std::ptrdiff_t>(actions.first),
                                        layout.actions.begin() + static_cast<std::ptrdiff_t>(actions.end),
                                        Outpost::Hud::ActionKind::EditName, [](const auto& _entry) { return _entry.second.kind; });
    Assert::IsTrue(name != layout.actions.begin() + static_cast<std::ptrdiff_t>(actions.end));
    Assert::AreEqual(size_t{1}, layout.LayerAt(name->first.left + 5.0f, name->first.top + 5.0f));
    Assert::IsTrue(layout.WindowAt(name->first.left + 5.0f, name->first.top + 5.0f) == &window);
    Assert::IsTrue(layout.Covers(window.frame.left + 1.0f, window.frame.top + 1.0f));
  }

  // ADR-031: a dragged window is laid out where it was left; a HUD button under it takes no click; and however far it is
  // dragged, and on whatever screen, its title bar stays where it can be taken hold of again.
  TEST_METHOD(KeepsAMovedWindowOnTheScreenAndInFront)
  {
    Outpost::WindowManager windows;
    const Outpost::Hud::Layout hudOnly = Lay(WithDesigner(), 1920, 1080, {}, &windows);
    const auto build =
      std::ranges::find(hudOnly.actions, Outpost::Hud::ActionKind::Build, [](const auto& _entry) { return _entry.second.kind; });
    Assert::IsTrue(build != hudOnly.actions.end());
    const float buildX = build->first.left + 5.0f;
    const float buildY = build->first.top + 5.0f;
    Assert::IsTrue(hudOnly.ActionAt(buildX, buildY).has_value());

    // At 1920x1080 a reference unit is a pixel. The window is moved so that its body covers the HUD's button.
    windows.Open(Outpost::WindowKind::Designer);
    const Outpost::WindowManager::Point over{.xUnits = buildX - 20.0f, .yUnits = buildY - 60.0f};
    windows.Grab(Outpost::WindowKind::Designer, over, over);
    windows.Release();
    const Outpost::Hud::Layout covered = Lay(WithDesigner(), 1920, 1080, {}, &windows);
    const Outpost::Hud::Window& window = covered.windows.front();
    Assert::IsTrue(window.frame.Contains(buildX, buildY));
    Assert::AreEqual(size_t{1}, covered.LayerAt(buildX, buildY));
    const std::optional<Outpost::Hud::Action> under = covered.ActionAt(buildX, buildY);
    Assert::IsTrue(!under.has_value() || under->kind != Outpost::Hud::ActionKind::Build, L"the window's, not the HUD's");

    // Dragged off the bottom-right corner, it keeps its title bar and some of its width on the screen.
    windows.Grab(Outpost::WindowKind::Designer, {}, {});
    windows.Drag({.xUnits = 5000.0f, .yUnits = 5000.0f});
    windows.Release();
    const Outpost::Hud::Window farWindow = Lay(WithDesigner(), 1920, 1080, {}, &windows).windows.front();
    Assert::AreEqual(1920.0f - Outpost::Hud::WINDOW_KEPT_ON_SCREEN_UNITS, farWindow.frame.left, 0.01f);
    Assert::AreEqual(1080.0f - Outpost::Hud::TITLE_BAR_UNITS, farWindow.frame.top, 0.01f);
    // And off the top-left one.
    windows.Grab(Outpost::WindowKind::Designer, {}, {});
    windows.Drag({.xUnits = -5000.0f, .yUnits = -5000.0f});
    windows.Release();
    const Outpost::Hud::Window nearWindow = Lay(WithDesigner(), 1280, 720, {}, &windows).windows.front();
    Assert::AreEqual(Outpost::Hud::WINDOW_KEPT_ON_SCREEN_UNITS * (720.0f / 1080.0f), nearWindow.frame.left + nearWindow.frame.width, 0.01f);
    Assert::AreEqual(0.0f, nearWindow.frame.top, 0.01f);
  }

  TEST_METHOD(KeepsAWindowsTitleBarOnTheScreen)
  {
    using Point = Outpost::WindowManager::Point;
    const auto kept = [](Point _corner) { return Outpost::Hud::KeepOnScreen(_corner, 600.0f, 1920.0f, 1080.0f); };
    Assert::IsTrue(kept({.xUnits = 100.0f, .yUnits = 100.0f}) == Point{.xUnits = 100.0f, .yUnits = 100.0f}, L"on the screen, it stays");
    Assert::IsTrue(kept({.xUnits = 3000.0f, .yUnits = 3000.0f}) == Point{.xUnits = 1800.0f, .yUnits = 1044.0f});
    Assert::IsTrue(kept({.xUnits = -3000.0f, .yUnits = -30.0f}) == Point{.xUnits = -480.0f, .yUnits = 0.0f});
  }

  TEST_METHOD(GroupsDigitsInThousands)
  {
    Assert::AreEqual(std::string("0"), Outpost::WithThousands(0));
    Assert::AreEqual(std::string("999"), Outpost::WithThousands(999));
    Assert::AreEqual(std::string("1,000"), Outpost::WithThousands(1000));
    Assert::AreEqual(std::string("12,345,678"), Outpost::WithThousands(12345678));
    Assert::AreEqual(std::string("-4,500"), Outpost::WithThousands(-4500));
  }

  // Task 3.6: the Ore stockpile, and no selection panel with nothing selected.
  TEST_METHOD(ShowsTheOreAndNoPanelWithoutASelection)
  {
    const Outpost::Hud::Content content = Outpost::Hud::Describe(Newest(), {}, {});
    Assert::AreEqual(12345, content.ore);
    Assert::IsTrue(content.selection.empty());
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(size_t{1}, layout.panels.size());
    Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "12,345"; }));
  }

  // One ship: its design's name and its hit points, rounded up to whole points.
  TEST_METHOD(DescribesOneShip)
  {
    const std::vector<Outpost::EntityView> entities{Ship(7, LINE, 44950, 45000)};
    const std::vector<Outpost::EntityId> selected{Outpost::EntityId{7}};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(Newest(), entities, selected);
    const std::vector<std::string> expected{"Medium+Ion+Lance", "Hit points 450 / 450"};
    Assert::IsTrue(content.selection == expected);
    Assert::AreEqual(44950.0f / 45000.0f, content.selectionHealth.value_or(-1.0f), 1e-6f);
  }

  // Several ships: how many, how many of each design, most first, and their hit points together.
  TEST_METHOD(DescribesAGroupByDesign)
  {
    const std::vector<Outpost::EntityView> entities{Ship(1, LINE, 45000, 45000), Ship(2, SWARM, 10000, 19800), Ship(3, SWARM, 19800, 19800),
                                                    Ship(4, SWARM, 19800, 19800)};
    const std::vector<Outpost::EntityId> selected{Outpost::EntityId{1}, Outpost::EntityId{2}, Outpost::EntityId{3}, Outpost::EntityId{4}};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(Newest(), entities, selected);
    // ADR-046: the count and the design between a multiplication sign.
    const std::vector<std::string> expected{"4 ships", "3 \xC3\x97 Small+Ion+Mass Driver", "1 \xC3\x97 Medium+Ion+Lance",
                                            "Hit points 946 / 1,044"};
    Assert::IsTrue(content.selection == expected);
    Assert::AreEqual(94600.0f / 104400.0f, content.selectionHealth.value_or(-1.0f), 1e-6f);
  }

  // ADR-006: one uniform scale, the largest at which the whole 1920x1080 frame fits, and anchors that keep the panels at
  // their corners and edges at any aspect ratio.
  TEST_METHOD(ScalesTheReferenceFrameToFit)
  {
    Assert::AreEqual(1.0f, Outpost::Hud::Scale(1920, 1080));
    Assert::AreEqual(2.0f, Outpost::Hud::Scale(3840, 2160));
    // A 16:10 panel: the width limits the scale.
    Assert::AreEqual(1.0f, Outpost::Hud::Scale(1920, 1200));

    const Outpost::Hud::Content content{.ore = 0, .selection = {"1 ship", "Hit points 1 / 1"}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1200);
    Assert::AreEqual(size_t{2}, layout.panels.size());
    const Outpost::Hud::Rect& selection = layout.panels[1];
    // Centered, and as far above the bottom edge as on a 1080-line screen.
    Assert::AreEqual(960.0f, selection.left + (selection.width / 2.0f), 0.01f);
    Assert::AreEqual(1200.0f - 16.0f, selection.top + selection.height, 0.01f);
    Assert::AreEqual(20.0f, layout.fontPixels, 0.01f);
  }

  // Task 4.5: what the rigs earn, each second, beside the stockpile.
  TEST_METHOD(ShowsTheIncome)
  {
    const auto incomeText = [](std::int32_t _hundredths)
    {
      const Outpost::Hud::Layout layout = Lay({.ore = 0, .oreIncomeHundredthsPerSecond = _hundredths}, 1920, 1080);
      const auto income = std::ranges::find_if(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text.ends_with("/s"); });
      return income != layout.texts.end() ? income->text : std::string();
    };
    Assert::AreEqual(std::string("+15/s"), incomeText(1500));
    Assert::AreEqual(std::string("+6.5/s"), incomeText(650));
    Assert::AreEqual(std::string("+0/s"), incomeText(0));
  }

  // An income of nothing is written in the warning's color: no rig earns, so nothing the player spends comes back.
  TEST_METHOD(WarnsOfNoIncome)
  {
    const auto incomeColor = [](std::int32_t _hundredths)
    {
      const Outpost::Hud::Layout layout = Lay({.ore = 0, .oreIncomeHundredthsPerSecond = _hundredths}, 1920, 1080);
      const auto income = std::ranges::find_if(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text.ends_with("/s"); });
      Assert::IsTrue(income != layout.texts.end());
      return income->color;
    };
    const DirectX::XMFLOAT4 earning = incomeColor(650);
    const DirectX::XMFLOAT4 nothing = incomeColor(0);
    Assert::IsTrue(earning.z > earning.x, L"an income is in the figures' blue");
    Assert::IsTrue(nothing.x > nothing.z, L"no income is in a warm warning color");
  }

  // Task 4.2: selected Constructors offer every structure they build, with its cost, dim when the player cannot afford
  // it or already has its one Research Lab; the Command Station is not built.
  TEST_METHOD(OffersTheStructuresToConstructors)
  {
    Outpost::Snapshot newest = Newest();
    newest.ore = 250;
    newest.structureTypes = {{.structure = Outpost::StructureKind::CommandStation, .nameUtf8 = "Command Station", .buildable = false},
                             {.structure = Outpost::StructureKind::Shipyard, .nameUtf8 = "Shipyard", .buildable = true, .cost = 300},
                             {.structure = Outpost::StructureKind::ResearchLab, .nameUtf8 = "Research Lab", .buildable = true, .cost = 200},
                             {.structure = Outpost::StructureKind::MiningRig, .nameUtf8 = "Mining Rig", .buildable = true, .cost = 50}};
    Outpost::EntityView constructor = Ship(9, {}, 30000, 30000);
    constructor.role = Outpost::ShipRole::Constructor;
    std::vector<Outpost::EntityView> entities{constructor};
    const std::vector<Outpost::EntityId> selected{Outpost::EntityId{9}};

    Outpost::Hud::Content content = Outpost::Hud::Describe(newest, entities, selected);
    Assert::AreEqual(std::string("Constructor"), content.selection.front());
    Assert::AreEqual(size_t{3}, content.buttons.size());
    Assert::AreEqual(std::string("Shipyard|300"), content.buttons[0].label);
    Assert::IsFalse(content.buttons[0].enabled, L"250 Ore does not buy a Shipyard");
    Assert::IsTrue(content.buttons[1].enabled);
    Assert::IsTrue(content.buttons[2].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Build, .structure = Outpost::StructureKind::MiningRig});

    entities.push_back({.id = Outpost::EntityId{20},
                        .kind = Outpost::EntityKind::Structure,
                        .owner = PLAYER,
                        .structure = Outpost::StructureKind::ResearchLab});
    content = Outpost::Hud::Describe(newest, entities, selected);
    Assert::IsFalse(content.buttons[1].enabled, L"one Research Lab a player");
    // ADR-046: the button says so in place of its cost; a button the player only cannot afford says nothing more.
    Assert::AreEqual(std::string("ONE PER PLAYER"), content.buttons[1].note);
    Assert::IsTrue(content.buttons[0].note.empty());
    const Outpost::Hud::Layout noted = Lay(content, 1920, 1080);
    Assert::IsTrue(std::ranges::any_of(noted.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "ONE PER PLAYER"; }));
    Assert::IsFalse(std::ranges::any_of(noted.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "200"; }));

    // While a placement is armed, a hint says what a click does.
    content = Outpost::Hud::Describe(newest, entities, selected, Outpost::StructureKind::MiningRig);
    Assert::AreEqual(std::string("Placing Mining Rig: left-click to build, right-click to cancel"), content.hint);
  }

  // ADR-056: a Constructor offers the Relay only on a map with sectors, which it holds.
  TEST_METHOD(OffersTheRelayOnlyWithSectors)
  {
    Outpost::Snapshot newest = Newest();
    newest.structureTypes = {{.structure = Outpost::StructureKind::Shipyard, .nameUtf8 = "Shipyard", .buildable = true, .cost = 300},
                             {.structure = Outpost::StructureKind::Relay, .nameUtf8 = "Relay", .buildable = true, .cost = 200}};
    Outpost::EntityView constructor = Ship(9, {}, 30000, 30000);
    constructor.role = Outpost::ShipRole::Constructor;
    const std::vector<Outpost::EntityView> entities{constructor};
    const std::vector<Outpost::EntityId> selected{Outpost::EntityId{9}};
    Assert::AreEqual(size_t{1}, Outpost::Hud::Describe(newest, entities, selected).buttons.size());

    newest.sectors = {{.id = 1, .nameUtf8 = "South", .maxXMeters = 100.0f, .maxZMeters = 100.0f, .holder = PLAYER}};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, entities, selected);
    Assert::AreEqual(size_t{2}, content.buttons.size());
    Assert::IsTrue(content.buttons[1].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Build, .structure = Outpost::StructureKind::Relay});
  }

  // ADR-056: the minimap washes each held sector in its holder's color under the marks, outlines it over the fog, and
  // stripes a suppressed one; a panel beside the Ore counts the nodes each side holds.
  TEST_METHOD(ShowsTheTerritory)
  {
    Outpost::Snapshot newest = Newest();
    newest.mapSizeMeters = 3000.0f;
    newest.sectors = {
      {.id = 1, .minXMeters = -1500.0f, .maxXMeters = -500.0f, .minZMeters = -1500.0f, .maxZMeters = 1500.0f, .holder = PLAYER},
      {.id = 2, .minXMeters = -500.0f, .maxXMeters = 500.0f, .minZMeters = -1500.0f, .maxZMeters = 1500.0f},
      {.id = 3,
       .minXMeters = 500.0f,
       .maxXMeters = 1500.0f,
       .minZMeters = -1500.0f,
       .maxZMeters = 1500.0f,
       .holder = Outpost::PlayerId{2},
       .suppressed = true}};
    Outpost::Hud::Content content = Outpost::Hud::Describe(newest, {}, {});
    Assert::IsTrue(content.territory.has_value());
    const Outpost::Hud::Territory territory = content.territory.value_or(Outpost::Hud::Territory{});
    Assert::AreEqual(1, territory.ownNodes);
    Assert::AreEqual(1, territory.enemyNodes);
    Assert::AreEqual(3, territory.nodes);
    Assert::AreEqual(size_t{3}, content.sectors.size());
    Assert::IsTrue(content.sectors[0].side == Outpost::Hud::Side::Own && content.sectors[2].side == Outpost::Hud::Side::Enemy);

    content.fog = true;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "Nodes of 3"; }));
    Assert::IsFalse(std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "Tickets"; }),
                    L"no tickets without them");
    const auto fog =
      std::ranges::find_if(layout.panels, [](const Outpost::Hud::Rect& _panel) { return _panel.fill == Outpost::Hud::Fill::Fog; });
    const DirectX::XMFLOAT2 west = layout.MinimapPixelOf({.xMeters = -1000.0f, .zMeters = 0.0f});
    const DirectX::XMFLOAT2 middle = layout.MinimapPixelOf({.xMeters = 0.0f, .zMeters = 0.0f});
    const auto wash = std::ranges::find_if(layout.panels, [&](const Outpost::Hud::Rect& _panel)
                                           { return _panel.Contains(west.x, west.y) && _panel.width > 50.0f && _panel.color.w < 0.5f; });
    Assert::IsTrue(wash != layout.panels.end() && wash < fog, L"the player's sector is washed under the fog");
    Assert::IsFalse(
      std::ranges::any_of(layout.panels, [&](const Outpost::Hud::Rect& _panel)
                          { return _panel.Contains(middle.x, middle.y) && _panel.width > 50.0f && _panel.width < layout.minimap.width; }),
      L"a free sector is not washed");
    const auto stripes = std::ranges::find_if(layout.panels, [](const Outpost::Hud::Rect& _panel)
                                              { return _panel.fill == Outpost::Hud::Fill::Hatched && _panel.width > 50.0f; });
    Assert::IsTrue(stripes != layout.panels.end() && stripes > fog, L"the suppressed sector is striped over the fog");
  }

  // Task 4.5, Phase 1 design §12: a selected structure shows its kind, hit points and construction; its queue and what it
  // can add to it are its windows', which its panel opens once it is the player's own and finished (owner, 2026-10-03).
  TEST_METHOD(DescribesAStructureAndOpensItsWindows)
  {
    Outpost::Snapshot newest = Newest();
    newest.structureTypes = {{.structure = Outpost::StructureKind::Shipyard, .nameUtf8 = "Shipyard", .buildable = true, .cost = 300}};
    Outpost::EntityView yard{.id = Outpost::EntityId{30},
                             .kind = Outpost::EntityKind::Structure,
                             .owner = PLAYER,
                             .structure = Outpost::StructureKind::Shipyard,
                             .hitPointsHundredths = 250000,
                             .maxHitPointsHundredths = 250000,
                             .queue = {{.design = SWARM}, {.design = LINE}},
                             .jobPermille = 455};
    const std::vector<Outpost::EntityId> selected{yard.id};
    const auto describe = [&](const Outpost::EntityView& _entity)
    { return Outpost::Hud::Describe(newest, std::vector{_entity}, selected); };
    Outpost::Hud::Content content = describe(yard);
    const std::vector<std::string> expected{"Shipyard", "Hit points 2,500 / 2,500"};
    Assert::IsTrue(content.selection == expected, L"the queue is the production window's");
    Assert::AreEqual(size_t{2}, content.buttons.size());
    Assert::IsTrue(content.buttons[0].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::OpenProduction, .producer = yard.id});
    Assert::IsTrue(content.buttons[1].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::OpenDesigner, .producer = yard.id});

    Outpost::EntityView station = yard;
    station.structure = Outpost::StructureKind::CommandStation;
    content = describe(station);
    Assert::AreEqual(size_t{1}, content.buttons.size());
    Assert::IsTrue(content.buttons[0].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::OpenProduction, .producer = station.id});

    Outpost::EntityView lab = yard;
    lab.structure = Outpost::StructureKind::ResearchLab;
    content = describe(lab);
    Assert::AreEqual(size_t{1}, content.buttons.size());
    Assert::IsTrue(content.buttons[0].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::OpenResearch, .producer = lab.id});

    Outpost::EntityView theirs = yard;
    theirs.owner = Outpost::PlayerId{2};
    Assert::IsTrue(describe(theirs).buttons.empty(), L"not the enemy's");

    // Under construction, it says how far it has come, and opens nothing.
    yard.builtPermille = 600;
    content = describe(yard);
    Assert::AreEqual(std::string("Under construction, 60%"), content.selection[1]);
    Assert::IsTrue(content.buttons.empty());
  }

  // Phase 1 design §12: the production window shows one producer: the Command Station's Constructor or a Shipyard's saved
  // designs, each with its abbreviation, dim once the queue is full or the Ore is short; the queue with the front job's
  // progress or its wait for Ore; and with no producer, or no design, how to get one.
  TEST_METHOD(DescribesTheProductionWindow)
  {
    Outpost::Snapshot newest = DesignerSnapshot();
    newest.constructorCost = 150;
    newest.entities.front().queue = {{.design = SWARM}};
    newest.entities.front().jobPermille = 455;
    const Outpost::EntityView station{.id = Outpost::EntityId{20},
                                      .kind = Outpost::EntityKind::Structure,
                                      .owner = PLAYER,
                                      .structure = Outpost::StructureKind::CommandStation,
                                      .queue = {{.role = Outpost::ShipRole::Constructor}}};
    newest.entities.push_back(station);

    Outpost::Hud::ProductionPanel panel = Outpost::Hud::DescribeProduction(newest, &newest.entities.front());
    Assert::AreEqual(std::string("SHIPYARD 01"), panel.producer);
    Assert::IsTrue(panel.hasProducer && panel.canStep);
    Assert::AreEqual(500, panel.ore);
    Assert::AreEqual(size_t{1}, panel.queue.size());
    Assert::AreEqual(std::string("Swarm"), panel.queue[0].name);
    Assert::IsTrue(panel.queue[0].front && !panel.queue[0].waiting);
    Assert::AreEqual(455, panel.queue[0].permille);
    Assert::AreEqual(size_t{1}, panel.options.size());
    Assert::AreEqual(std::string("S\xC2\xB7I\xC2\xB7MD"), panel.options[0].detail);
    Assert::AreEqual(87, panel.options[0].cost);
    Assert::IsTrue(panel.options[0].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Queue, .producer = Outpost::EntityId{30}, .design = SWARM});
    Assert::IsTrue(panel.options[0].enabled);
    Assert::IsTrue(panel.hint.empty());

    newest.entities.front().queue.resize(Outpost::QUEUE_LIMIT, {.design = SWARM});
    Assert::IsFalse(Outpost::Hud::DescribeProduction(newest, &newest.entities.front()).options[0].enabled, L"the queue is full");

    panel = Outpost::Hud::DescribeProduction(newest, &newest.entities.back());
    Assert::AreEqual(std::string("COMMAND STATION"), panel.producer);
    Assert::AreEqual(std::string("Constructor"), panel.queue[0].name);
    Assert::IsTrue(panel.queue[0].waiting, L"nothing done yet");
    Assert::AreEqual(size_t{1}, panel.options.size());
    Assert::IsTrue(panel.options[0].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Queue, .producer = Outpost::EntityId{20}});
    Assert::AreEqual(150, panel.options[0].cost);
    newest.ore = 149;
    Assert::IsFalse(Outpost::Hud::DescribeProduction(newest, &newest.entities.back()).options[0].enabled, L"short of Ore");

    newest.designs.clear();
    Assert::AreEqual(std::string("Save a design in the ship designer to build it here."),
                     Outpost::Hud::DescribeProduction(newest, &newest.entities.front()).hint);
    panel = Outpost::Hud::DescribeProduction(newest, nullptr);
    Assert::AreEqual(std::string("NO PRODUCER"), panel.producer);
    Assert::IsFalse(panel.hasProducer);
    Assert::IsTrue(panel.options.empty() && !panel.hint.empty());
  }

  // Task 4.5: an enabled button is a place to click, anchored to the bottom-right corner; a dim one is not.
  TEST_METHOD(LaysOutButtonsForClicks)
  {
    const Outpost::Hud::Action build{.kind = Outpost::Hud::ActionKind::Build, .structure = Outpost::StructureKind::Shipyard};
    const Outpost::Hud::Content content{
      .ore = 0, .buttons = {{.label = "Shipyard|300", .action = build}, {.label = "Research Lab|200", .action = build, .enabled = false}}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(size_t{1}, layout.actions.size());
    const Outpost::Hud::Rect& area = layout.actions.front().first;
    Assert::IsTrue(area.left + area.width < 1920.0f && area.left > 1920.0f - 400.0f);
    Assert::IsTrue(layout.ActionAt(area.left + 5.0f, area.top + 5.0f) == build);
    Assert::IsFalse(layout.ActionAt(area.left + 5.0f, area.top + area.height + 10.0f).has_value());
    Assert::IsTrue(layout.Covers(area.left + 5.0f, area.top + 5.0f));
  }

  // Phase 1 design §8: a Mining Rig's panel says how much Ore is left in its asteroid, as far as the player knows, and the
  // minimap draws an asteroid that has run out darker than one that has not.
  TEST_METHOD(ShowsTheOreLeft)
  {
    Outpost::Snapshot newest = Newest();
    newest.mapSizeMeters = 2000.0f;
    newest.structureTypes = {{.structure = Outpost::StructureKind::MiningRig, .nameUtf8 = "Mining Rig", .buildable = true, .cost = 50}};
    Outpost::EntityView rig{.id = Outpost::EntityId{30},
                            .kind = Outpost::EntityKind::Structure,
                            .owner = PLAYER,
                            .structure = Outpost::StructureKind::MiningRig,
                            .hitPointsHundredths = 80000,
                            .maxHitPointsHundredths = 80000,
                            .oreReserveHundredths = 342000};
    const std::vector<Outpost::EntityId> selected{rig.id};
    std::vector<std::string> expected{"Mining Rig", "Hit points 800 / 800", "Ore left 3,420"};
    Assert::IsTrue(Outpost::Hud::Describe(newest, std::vector{rig}, selected).selection == expected);
    rig.oreReserveHundredths = 0;
    expected.back() = "Ore run out: it earns a trickle";
    Assert::IsTrue(Outpost::Hud::Describe(newest, std::vector{rig}, selected).selection == expected);
    rig.oreReserveHundredths.reset();
    expected.pop_back();
    Assert::IsTrue(Outpost::Hud::Describe(newest, std::vector{rig}, selected).selection == expected, L"one it has not seen");

    const Outpost::EntityView full{.id = Outpost::EntityId{1},
                                   .kind = Outpost::EntityKind::Asteroid,
                                   .position = {-500.0f, 0.0f},
                                   .radiusMeters = 45.0f,
                                   .oreReserveHundredths = 100};
    Outpost::EntityView dry = full;
    dry.id = Outpost::EntityId{2};
    dry.position = {500.0f, 0.0f};
    dry.oreReserveHundredths = 0;
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, std::vector{full, dry}, {});
    Assert::IsFalse(content.marks[0].dry);
    Assert::IsTrue(content.marks[1].dry);
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const auto colorAt = [&layout](Outpost::PlanePosition _position)
    {
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(_position);
      const auto panel = std::ranges::find_if(layout.panels, [&at](const Outpost::Hud::Rect& _rect)
                                              { return _rect.Contains(at.x, at.y) && _rect.width < 40.0f; });
      Assert::IsTrue(panel != layout.panels.end());
      return panel->color;
    };
    Assert::IsTrue(colorAt(dry.position).x < colorAt(full.position).x, L"darker");
  }

  // Task 4.5: the minimap shows the map's square, +x to the right and +z up, and a point on it is a point on the map.
  TEST_METHOD(MapsTheMinimapToTheMap)
  {
    Outpost::Snapshot newest = Newest();
    newest.mapSizeMeters = 2000.0f;
    const std::vector<Outpost::EntityView> entities{Ship(1, LINE, 1, 1)};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, entities, {});
    Assert::AreEqual(size_t{1}, content.marks.size());
    const std::vector<Outpost::PlanePosition> view{{-200.0f, 150.0f}, {200.0f, 150.0f}, {300.0f, -150.0f}, {-300.0f, -150.0f}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080, view);
    const Outpost::Hud::Rect& map = layout.minimap;
    Assert::IsTrue(map.width > 0.0f && map.left < 400.0f && map.top + map.height > 800.0f, L"bottom-left");

    const Outpost::PlanePosition center =
      layout.MapPointAt(map.left + (map.width / 2.0f), map.top + (map.height / 2.0f)).value_or(Outpost::PlanePosition{1e9f, 1e9f});
    Assert::AreEqual(0.0f, center.xMeters, 0.5f);
    Assert::AreEqual(0.0f, center.zMeters, 0.5f);
    const DirectX::XMFLOAT2 northEast = layout.MinimapPixelOf({1000.0f, 1000.0f});
    Assert::AreEqual(map.left + map.width, northEast.x, 0.01f);
    Assert::AreEqual(map.top, northEast.y, 0.01f);
    Assert::IsFalse(layout.MapPointAt(map.left - 5.0f, map.top).has_value());
  }

  // Task 5.1, Phase 1 design §12: the research window shows the Research Lab's queue, and offers each topic not researched
  // or queued yet with what it does and what it costs, dim while its prerequisite is neither; the topic under way shows
  // under the Ore.
  TEST_METHOD(DescribesTheResearchLab)
  {
    Outpost::Snapshot newest = Newest();
    newest.ore = 250;
    newest.research = {
      {.id = Outpost::ResearchTopicId{1}, .nameUtf8 = "Improved Extraction", .effectUtf8 = "Mining Rig income +25%", .cost = 150},
      {.id = Outpost::ResearchTopicId{2}, .nameUtf8 = "Hull Plating", .effectUtf8 = "Hull hit points +15%", .cost = 150},
      {.id = Outpost::ResearchTopicId{5},
       .nameUtf8 = "Fusion Drive",
       .effectUtf8 = "Unlocks the Fusion drive",
       .cost = 200,
       .prerequisites = {Outpost::ResearchTopicId{2}}},
      {.id = Outpost::ResearchTopicId{8},
       .nameUtf8 = "Automated Shipyards",
       .effectUtf8 = "Shipyard build speed +25%",
       .cost = 200,
       .prerequisites = {Outpost::ResearchTopicId{1}},
       .researched = false}};
    newest.research[0].researched = true;
    newest.structureTypes = {
      {.structure = Outpost::StructureKind::ResearchLab, .nameUtf8 = "Research Lab", .buildable = true, .cost = 200}};
    const Outpost::EntityView lab{.id = Outpost::EntityId{40},
                                  .kind = Outpost::EntityKind::Structure,
                                  .owner = PLAYER,
                                  .structure = Outpost::StructureKind::ResearchLab,
                                  .hitPointsHundredths = 150000,
                                  .maxHitPointsHundredths = 150000,
                                  .research = {Outpost::ResearchTopicId{2}},
                                  .jobPermille = 250};
    const Outpost::Hud::ResearchPanel panel = Outpost::Hud::DescribeResearch(newest, std::vector{lab});
    Assert::AreEqual(std::string("RESEARCH LAB"), panel.lab);
    Assert::IsTrue(panel.hasLab);
    Assert::AreEqual(size_t{1}, panel.queue.size());
    Assert::AreEqual(std::string("Hull Plating"), panel.queue[0].name);
    Assert::AreEqual(250, panel.queue[0].permille);
    Assert::AreEqual(size_t{2}, panel.topics.size(), L"neither the researched topic nor the queued one");
    const Outpost::Hud::TopicCard& fusion = panel.topics[0];
    Assert::AreEqual(std::string("Fusion Drive"), fusion.name);
    Assert::AreEqual(std::string("Unlocks the Fusion drive"), fusion.effect);
    Assert::AreEqual(200, fusion.cost);
    Assert::IsTrue(fusion.enabled && fusion.needs.empty(), L"its prerequisite is queued");
    Assert::IsTrue(panel.topics[1].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Research,
                                                                  .producer = lab.id,
                                                                  .topic = Outpost::ResearchTopicId{8}});
    Assert::IsTrue(panel.topics[1].enabled, L"its prerequisite is researched");

    // A topic whose prerequisite is neither researched nor queued is dim and names it; so is every topic without a Lab.
    newest.research[0].researched = false;
    const Outpost::Hud::ResearchPanel waiting = Outpost::Hud::DescribeResearch(newest, std::vector{lab});
    const auto automated = std::ranges::find(waiting.topics, std::string("Automated Shipyards"), &Outpost::Hud::TopicCard::name);
    Assert::IsTrue(automated != waiting.topics.end());
    Assert::IsTrue(automated->needs == std::vector<std::string>{"IMPROVED EXTRACTION"});
    Assert::IsFalse(automated->enabled);
    const Outpost::Hud::ResearchPanel none = Outpost::Hud::DescribeResearch(newest, {});
    Assert::AreEqual(std::string("NO RESEARCH LAB"), none.lab);
    Assert::IsTrue(std::ranges::none_of(none.topics, &Outpost::Hud::TopicCard::enabled));
    newest.research[0].researched = true;

    // Nothing selected, the research still shows under the Ore.
    const Outpost::Hud::Content unselected = Outpost::Hud::Describe(newest, std::vector{lab}, {});
    Assert::AreEqual(std::string("Researching Hull Plating, 25%"), unselected.research);
    const Outpost::Hud::Layout layout = Lay(unselected, 1920, 1080);
    Assert::IsTrue(
      std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "Researching Hull Plating, 25%"; }));
  }

  // Phase 1 design §5, §11: a component's abbreviation is the capital initial of each word of its name.
  TEST_METHOD(AbbreviatesComponentsByTheirInitials)
  {
    Assert::AreEqual(std::string("S"), Outpost::Abbreviation("Small"));
    Assert::AreEqual(std::string("MD"), Outpost::Abbreviation("Mass Driver"));
    Assert::AreEqual(std::string("RC"), Outpost::Abbreviation("rail cannon"));
    Assert::AreEqual(std::string(""), Outpost::Abbreviation(""));
  }

  // Phase 1 design §11: the designer belongs to the player, not to a Shipyard: it is described whenever GameClient gives
  // it, and a selected finished Shipyard of the player's offers to open it there.
  TEST_METHOD(OffersTheDesignerAtThePlayersShipyards)
  {
    const Outpost::Snapshot newest = DesignerSnapshot();
    const Outpost::EntityView& yard = newest.entities.front();
    const std::vector<Outpost::EntityId> selected{yard.id};
    Assert::IsFalse(Outpost::Hud::Describe(newest, newest.entities, selected).designer.has_value(), L"no designer given");
    const auto opens = [&](const Outpost::EntityView& _yard)
    {
      const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, std::vector{_yard}, selected);
      return std::ranges::any_of(
        content.buttons, [&](const Outpost::Hud::Button& _button)
        { return _button.action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::OpenDesigner, .producer = yard.id}; });
    };
    Assert::IsTrue(opens(yard));
    Outpost::EntityView building = yard;
    building.builtPermille = 500;
    Assert::IsFalse(opens(building), L"not until it is finished");
    Outpost::EntityView theirs = yard;
    theirs.owner = Outpost::PlayerId{2};
    Assert::IsFalse(opens(theirs), L"not the enemy's");

    Outpost::Designer designer;
    designer.Update(newest);
    Assert::IsTrue(Outpost::Hud::Describe(newest, newest.entities, {}, std::nullopt, &designer).designer.has_value(), L"nothing selected");
  }

  // Phase 1 design §11, after the mockup: the header's Shipyard, queue, ships built and Ore; the name and Saved; the saved
  // design's chip; a card for each component with its numbers, the locked ones naming their research; the bars against the
  // best any design reaches, locked components included; the damage cards rated by thirds; and Queue.
  TEST_METHOD(DescribesTheDesignerAfterTheMockup)
  {
    const Outpost::Snapshot newest = DesignerSnapshot();
    Outpost::Designer designer;
    designer.Update(newest);
    const Outpost::Hud::DesignerPanel panel = DesignerOf(newest, designer);
    Assert::AreEqual(std::string("SHIPYARD 01"), panel.shipyard);
    Assert::IsTrue(panel.hasShipyard);
    Assert::AreEqual(2u, panel.queued);
    Assert::AreEqual(4u, panel.built);
    Assert::AreEqual(500, panel.ore);
    Assert::AreEqual(std::string("Swarm"), panel.name);
    Assert::AreEqual(std::string("SAVED"), panel.save.label);
    Assert::IsTrue(panel.save.selected && !panel.save.enabled);
    Assert::IsFalse(panel.rename.enabled, L"the name has not changed");

    Assert::AreEqual(size_t{1}, panel.chips.size());
    Assert::AreEqual(std::string("S\xC2\xB7I\xC2\xB7MD"), panel.chips[0].code);
    Assert::IsTrue(panel.chips[0].shown);
    Assert::IsTrue(panel.chips[0].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::LoadDesign, .design = SWARM});

    const Outpost::Hud::SlotRow& hulls = panel.slots[0];
    Assert::AreEqual(std::string("HULL"), hulls.label);
    Assert::AreEqual(std::string("Small"), hulls.picked);
    Assert::AreEqual(size_t{3}, hulls.cards.size());
    Assert::IsTrue(hulls.cards[0].picked && !hulls.cards[0].IsLocked());
    Assert::AreEqual(std::string("220 HP \xC2\xB7 ARM 2 \xC2\xB7 60m/s"), hulls.cards[0].numbers);
    Assert::AreEqual(std::string("RESEARCH \xC2\xB7 LARGE HULL"), hulls.cards[2].lockedBy);
    Assert::AreEqual(std::string("SPD \xC3\x97") + "1.3 \xC2\xB7 HP \xC3\x97" + "0.9", panel.slots[1].cards[0].numbers);
    const Outpost::Hud::PartCard& missiles = panel.slots[2].cards[2];
    Assert::AreEqual(std::string("30 dmg / 2s \xC2\xB7 280m"), missiles.numbers);
    Assert::AreEqual(std::string("splash 30m"), missiles.note);

    // Small+Ion+Mass Driver against the best of all: 1,680 hit points, armor 14, 78 m/s, 280 m, 510 Ore and 32 s.
    Assert::AreEqual(size_t{6}, panel.bars.size());
    const std::array<std::string, 6> values{"198", "2", "78", "120", "87", "8"};
    const std::array<float, 6> shares{198.0f / 1680.0f, 2.0f / 14.0f, 1.0f, 120.0f / 280.0f, 87.0f / 510.0f, 8.0f / 32.0f};
    for (size_t i = 0; i < panel.bars.size(); ++i)
    {
      Assert::AreEqual(values[i], panel.bars[i].value);
      Assert::AreEqual(shares[i], panel.bars[i].share, 1e-4f);
      Assert::IsFalse(panel.bars[i].previewShare.has_value());
    }
    Assert::AreEqual(std::string("m/s"), panel.bars[2].unit);

    // Against each hull, per ship and per 100 Ore, and rated against the best any design does to it: the Lance's 34.4 to
    // the Small hull, 32.2 to the Medium and 30.0 to the Large.
    Assert::AreEqual(size_t{3}, panel.damage.size());
    Assert::AreEqual(std::string("30.0"), panel.damage[0].perShip);
    Assert::AreEqual(std::string("34.5 / 100 ore"), panel.damage[0].perOre);
    Assert::IsTrue(panel.damage[0].rating == Outpost::Hud::Rating::Good);
    Assert::AreEqual(std::string("15.0"), panel.damage[1].perShip);
    Assert::IsTrue(panel.damage[1].rating == Outpost::Hud::Rating::Fair);
    Assert::AreEqual(std::string("8.8"), panel.damage[2].perShip);
    Assert::AreEqual(std::string("ARM 14"), panel.damage[2].armor);
    Assert::IsTrue(panel.damage[2].rating == Outpost::Hud::Rating::Poor);
    Assert::AreEqual(8.75f / 30.0f, panel.damage[2].share, 1e-4f);

    Assert::IsTrue(
      panel.queue.action ==
      Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Queue, .producer = Outpost::EntityId{30}, .design = SWARM, .count = 1});
    Assert::IsTrue(panel.queue.enabled);
    Assert::AreEqual(87, panel.queueCost);
    Assert::AreEqual(std::string("8 s each"), panel.queueDetail);
    Assert::AreEqual(std::string("Hover any part to preview its effect."), panel.hint);
  }

  // Phase 1 design §11: hovering a part previews the design it would make, and how each number changes; lower is better
  // for cost and build time. Hovering the pick previews nothing.
  TEST_METHOD(PreviewsAHoveredPart)
  {
    const Outpost::Snapshot newest = DesignerSnapshot();
    Outpost::Designer designer;
    designer.Update(newest);
    const Outpost::Hud::Action medium{.kind = Outpost::Hud::ActionKind::PickHull, .hull = Outpost::HullId{2}};
    const Outpost::Hud::DesignerPanel panel = DesignerOf(newest, designer, medium);
    Assert::AreEqual(std::string("Preview: with Medium instead"), panel.hint);
    using Change = Outpost::Hud::Change;
    const std::array<std::string, 6> values{"450", "8", "52", "120", "165", "16"};
    const std::array<Change, 6> changes{Change::Better, Change::Better, Change::Worse, Change::Same, Change::Worse, Change::Worse};
    for (size_t i = 0; i < panel.bars.size(); ++i)
    {
      Assert::AreEqual(values[i], panel.bars[i].previewValue);
      Assert::IsTrue(panel.bars[i].change == changes[i]);
      Assert::IsTrue(panel.bars[i].previewShare.has_value());
    }
    Assert::AreEqual(std::string("198"), panel.bars[0].value, L"the pick's own stays");
    Assert::AreEqual(450.0f / 1680.0f, panel.bars[0].previewShare.value_or(0.0f), 1e-4f);
    Assert::AreEqual(std::string("18.2 / 100 ore"), panel.damage[0].perOre, L"the damage cards show the preview");
    Assert::IsTrue(panel.damage[0].change == Change::Same, L"the same weapon");

    const Outpost::Hud::DesignerPanel same =
      DesignerOf(newest, designer, Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::PickHull, .hull = Outpost::HullId{1}});
    Assert::IsFalse(same.bars[0].previewShare.has_value());
  }

  // Phase 1 design §11: picks that are no saved design are saved first by Queue (ADR-023); ×N asks for N ships at N times
  // the cost, from 1 up to the target's free slots; and with no Shipyard, Queue is dim.
  TEST_METHOD(QueuesNShipsAtTheTarget)
  {
    Outpost::Snapshot newest = DesignerSnapshot(true);
    Outpost::Designer designer;
    designer.Update(newest);
    designer.PickHull(Outpost::HullId{3});
    Outpost::Hud::DesignerPanel panel = DesignerOf(newest, designer);
    Assert::AreEqual(std::string("SAVE"), panel.save.label);
    Assert::IsTrue(panel.save.enabled);
    Assert::IsFalse(panel.canFewer);
    Assert::IsTrue(panel.canMore);
    Assert::IsTrue(panel.queue.action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::SaveAndQueue, .producer = Outpost::EntityId{30}, .count = 1});
    Assert::IsTrue(panel.queue.enabled);
    Assert::AreEqual(355, panel.queueCost);

    for (int i = 0; i < 5; ++i)
      designer.StepCount(1, newest);
    panel = DesignerOf(newest, designer);
    Assert::AreEqual(3u, panel.count, L"three slots are free");
    Assert::IsTrue(panel.canFewer && !panel.canMore);
    Assert::AreEqual(3u, panel.queue.action.count);
    Assert::AreEqual(1065, panel.queueCost);
    Assert::IsTrue(panel.queue.enabled, L"each job is paid when it starts");

    newest.ore = 354;
    Assert::IsFalse(DesignerOf(newest, designer).queue.enabled, L"short of Ore for one");

    newest.ore = 500;
    newest.entities.clear();
    designer.Update(newest);
    panel = DesignerOf(newest, designer);
    Assert::AreEqual(std::string("NO SHIPYARD"), panel.shipyard);
    Assert::IsFalse(panel.hasShipyard);
    Assert::IsFalse(panel.queue.enabled);
    Assert::IsFalse(panel.canMore);
    Assert::IsTrue(panel.save.enabled, L"it still designs and saves");
  }

  // Phase 2 design §10: the production window shows a design's module after its components' initials, as the designer's
  // chips do, so that a scout reads apart from the plain design of its hull, drive and weapon.
  TEST_METHOD(NamesAModuleInTheProductionWindow)
  {
    Outpost::Snapshot newest = DesignerSnapshot();
    newest.modules = {{.id = Outpost::ModuleId{1}, .nameUtf8 = "Sensor Array", .sightMeters = 700.0, .speedFactor = 0.9, .cost = 40}};
    newest.designs.push_back({.id = Outpost::DesignId{2},
                              .nameUtf8 = "Scout",
                              .hull = Outpost::HullId{1},
                              .drive = Outpost::DriveId{1},
                              .weapon = Outpost::WeaponId{1},
                              .module = Outpost::ModuleId{1},
                              .cost = 127});
    const Outpost::Hud::ProductionPanel panel = Outpost::Hud::DescribeProduction(newest, &newest.entities.front());
    Assert::AreEqual(size_t{2}, panel.options.size());
    Assert::AreEqual(std::string("S\xC2\xB7I\xC2\xB7MD"), panel.options[0].detail);
    Assert::AreEqual(std::string("S\xC2\xB7I\xC2\xB7MD\xC2\xB7SA"), panel.options[1].detail);
  }

  // Phase 1 design §11, after the mockup: a window 728 units wide; the Shipyard's arrows in its title bar, pressed rather
  // than grabbed; a card for each unlocked part to click and none for a locked one, which is hatched; the weapons wrap to a
  // second line of cards once there are more than three; and Queue at the bottom.
  // Phase 2 design §10: the designer's fourth row holds no module and each module; a design's chip names its module's
  // initials; a Sensors bar shows how far a module lets the ship see; and the window still fits a 1080-line screen.
  TEST_METHOD(OffersTheModulesInAFourthRow)
  {
    constexpr Outpost::ModuleId SENSOR_ARRAY{1};
    Outpost::Snapshot newest = DesignerSnapshot(false, true);
    newest.modules = {
      {.id = SENSOR_ARRAY, .nameUtf8 = "Sensor Array", .sightMeters = 700.0, .speedFactor = 0.9, .cost = 40, .available = true}};
    newest.designs.push_back({.id = Outpost::DesignId{2},
                              .nameUtf8 = "Scout",
                              .hull = Outpost::HullId{1},
                              .drive = Outpost::DriveId{1},
                              .weapon = Outpost::WeaponId{1},
                              .module = SENSOR_ARRAY,
                              .cost = 127});
    Outpost::Designer designer;
    designer.Update(newest);
    Outpost::Hud::DesignerPanel panel = DesignerOf(newest, designer);
    const Outpost::Hud::SlotRow& modules = panel.slots[3];
    Assert::AreEqual(std::string("MODULE"), modules.label);
    Assert::AreEqual(std::string("None"), modules.picked);
    Assert::AreEqual(size_t{2}, modules.cards.size());
    Assert::IsTrue(modules.cards[0].picked && !modules.cards[1].picked);
    Assert::IsTrue(modules.cards[1].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::PickModule, .module = SENSOR_ARRAY});
    Assert::AreEqual(40, modules.cards[1].cost);
    Assert::AreEqual(std::string("S\xC2\xB7I\xC2\xB7MD\xC2\xB7SA"), panel.chips[1].code);
    const auto sensors = std::ranges::find(panel.bars, std::string("Sensors"), &Outpost::Hud::StatBar::label);
    Assert::IsTrue(sensors != panel.bars.end());
    Assert::AreEqual(std::string("-"), sensors->value);

    // Hovering the module previews the design it would make.
    panel = DesignerOf(newest, designer, modules.cards[1].action);
    Assert::AreEqual(std::string("Preview: with Sensor Array instead"), panel.hint);
    const auto previewed = std::ranges::find(panel.bars, std::string("Sensors"), &Outpost::Hud::StatBar::label);
    Assert::AreEqual(std::string("700"), previewed->previewValue);

    Outpost::Hud::Content content;
    content.designer = panel;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(size_t{1}, layout.windows.size());
    const Outpost::Hud::Rect& frame = layout.windows.front().frame;
    Assert::IsTrue(frame.top >= 0.0f && frame.top + frame.height <= 1080.0f, std::to_wstring(frame.top + frame.height).c_str());
  }

  TEST_METHOD(LaysTheDesignerOutAfterTheMockup)
  {
    const auto layOut = [](const Outpost::Snapshot& _newest)
    {
      Outpost::Designer designer;
      designer.Update(_newest);
      Outpost::Hud::Content content;
      content.designer = DesignerOf(_newest, designer);
      return Lay(content, 1920, 1080);
    };
    const auto count = [](const Outpost::Hud::Layout& _layout, Outpost::Hud::ActionKind _kind)
    { return static_cast<size_t>(std::ranges::count(_layout.actions, _kind, [](const auto& _entry) { return _entry.second.kind; })); };
    const auto areaOf = [](const Outpost::Hud::Layout& _layout, Outpost::Hud::ActionKind _kind, size_t _nth = 0)
    {
      for (const auto& [area, action] : _layout.actions)
      {
        if (action.kind == _kind && _nth-- == 0)
          return area;
      }
      return Outpost::Hud::Rect{};
    };

    const Outpost::Hud::Layout mockup = layOut(DesignerSnapshot());
    const Outpost::Hud::Window& window = mockup.windows.front();
    Assert::AreEqual(728.0f, window.frame.width, 0.01f);
    Assert::AreEqual(1920.0f - 16.0f - 728.0f, window.frame.left, 0.01f, L"top right");
    Assert::AreEqual(size_t{2}, count(mockup, Outpost::Hud::ActionKind::PickHull), L"the Large hull is locked");
    Assert::AreEqual(size_t{1}, count(mockup, Outpost::Hud::ActionKind::PickDrive));
    Assert::AreEqual(size_t{2}, count(mockup, Outpost::Hud::ActionKind::PickWeapon));
    Assert::AreEqual(size_t{1}, count(mockup, Outpost::Hud::ActionKind::LoadDesign));
    Assert::AreEqual(size_t{0}, count(mockup, Outpost::Hud::ActionKind::NextDesigns), L"one chip fits");
    Assert::IsTrue(std::ranges::any_of(mockup.panels, [](const Outpost::Hud::Rect& _panel)
                                       { return _panel.fill == Outpost::Hud::Fill::Hatched && _panel.width < 200.0f; }),
                   L"a locked card is hatched");

    const Outpost::Hud::Rect previous = areaOf(mockup, Outpost::Hud::ActionKind::PreviousShipyard);
    Assert::IsTrue(window.titleBar.Contains(previous.left + 2.0f, previous.top + 2.0f));
    Assert::IsTrue(mockup.ActionAt(previous.left + 2.0f, previous.top + 2.0f) ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::PreviousShipyard});
    const Outpost::Hud::Rect queue = areaOf(mockup, Outpost::Hud::ActionKind::Queue);
    Assert::IsTrue(queue.top > areaOf(mockup, Outpost::Hud::ActionKind::PickWeapon).top);
    Assert::IsTrue(queue.top + queue.height < window.frame.top + window.frame.height);

    // Every part unlocked and five weapons: the fourth and fifth on a second line, the window taller by it.
    const Outpost::Hud::Layout unlocked = layOut(DesignerSnapshot(true, true));
    const Outpost::Hud::Window& tall = unlocked.windows.front();
    Assert::AreEqual(size_t{3}, count(unlocked, Outpost::Hud::ActionKind::PickHull));
    Assert::AreEqual(size_t{5}, count(unlocked, Outpost::Hud::ActionKind::PickWeapon));
    const Outpost::Hud::Rect first = areaOf(unlocked, Outpost::Hud::ActionKind::PickWeapon, 0);
    const Outpost::Hud::Rect fourth = areaOf(unlocked, Outpost::Hud::ActionKind::PickWeapon, 3);
    Assert::AreEqual(first.left, fourth.left, 0.01f);
    Assert::IsTrue(fourth.top > first.top + first.height);
    Assert::AreEqual(fourth.top - first.top, tall.frame.height - window.frame.height, 0.01f);

    // Every part locked: nothing to pick, no numbers and nothing to queue.
    Outpost::Snapshot locked = DesignerSnapshot();
    for (Outpost::HullView& hull : locked.hulls)
      hull.available = false;
    for (Outpost::DriveView& drive : locked.drives)
      drive.available = false;
    for (Outpost::WeaponView& weapon : locked.weapons)
      weapon.available = false;
    const Outpost::Hud::Layout none = layOut(locked);
    Assert::AreEqual(size_t{0}, count(none, Outpost::Hud::ActionKind::PickHull) + count(none, Outpost::Hud::ActionKind::PickWeapon));
    Assert::AreEqual(size_t{0}, count(none, Outpost::Hud::ActionKind::Queue) + count(none, Outpost::Hud::ActionKind::SaveAndQueue));

    // More saved designs than fit: three chips, and an arrow on to the rest.
    Outpost::Snapshot many = DesignerSnapshot();
    for (std::uint32_t i = 2; i <= 5; ++i)
      many.designs.push_back({.id = Outpost::DesignId{i}, .nameUtf8 = std::format("Design {}", i), .hull = Outpost::HullId{2}});
    const Outpost::Hud::Layout chips = layOut(many);
    Assert::AreEqual(size_t{3}, count(chips, Outpost::Hud::ActionKind::LoadDesign));
    Assert::AreEqual(size_t{1}, count(chips, Outpost::Hud::ActionKind::NextDesigns));
    Assert::AreEqual(size_t{0}, count(chips, Outpost::Hud::ActionKind::PreviousDesigns), L"at the first");
  }

  // Phase 1 design §6: the research window lists the topics tier by tier, each with its tier, and marks a gateway.
  TEST_METHOD(ListsTheTopicsTierByTier)
  {
    Outpost::Snapshot newest = Newest();
    newest.ore = 1000;
    newest.research = {{.id = Outpost::ResearchTopicId{9},
                        .nameUtf8 = "Relay Archives",
                        .effectUtf8 = "Opens tier 2",
                        .cost = 400,
                        .researchSeconds = 150.0,
                        .tier = 2,
                        .gateway = true},
                       {.id = Outpost::ResearchTopicId{2},
                        .nameUtf8 = "Hull Plating",
                        .effectUtf8 = "Hull hit points +15%",
                        .cost = 150,
                        .researchSeconds = 60.0},
                       {.id = Outpost::ResearchTopicId{14},
                        .nameUtf8 = "Reinforced Structures",
                        .effectUtf8 = "Structure hit points +25%",
                        .cost = 250,
                        .researchSeconds = 90.0,
                        .prerequisites = {Outpost::ResearchTopicId{9}},
                        .tier = 2}};
    const Outpost::EntityView lab{.id = Outpost::EntityId{40},
                                  .kind = Outpost::EntityKind::Structure,
                                  .owner = PLAYER,
                                  .structure = Outpost::StructureKind::ResearchLab};
    const Outpost::Hud::ResearchPanel panel = Outpost::Hud::DescribeResearch(newest, std::vector{lab});
    Assert::AreEqual(size_t{3}, panel.topics.size());
    Assert::AreEqual(std::string("Hull Plating"), panel.topics[0].name, L"tier 1 first");
    Assert::AreEqual(std::string("TIER 1 \xC2\xB7 60 s"), panel.topics[0].time);
    Assert::IsTrue(panel.topics[1].gateway && panel.topics[1].tier == 2);
    Assert::AreEqual(std::string("TIER 2 \xC2\xB7 150 s"), panel.topics[1].time);
    Assert::IsTrue(panel.topics[2].needs == std::vector<std::string>{"RELAY ARCHIVES"}, L"its tier waits for the gateway");
    Assert::IsFalse(panel.topics[2].gateway);

    Outpost::Hud::Content content;
    content.laboratory = panel;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const Outpost::Hud::Span panels = layout.PanelsOf(1);
    const auto gold = std::count_if(layout.panels.begin() + static_cast<std::ptrdiff_t>(panels.first),
                                    layout.panels.begin() + static_cast<std::ptrdiff_t>(panels.end),
                                    [](const Outpost::Hud::Rect& _rect) { return _rect.color.x > 0.9f && _rect.color.y > 0.45f; });
    Assert::IsTrue(gold >= 4, L"the gateway's card is edged in gold");
  }

  // Phase 1 design §12: the production and research windows are laid out side by side under the Ore, clear of the
  // designer; each enabled card is a place to click, and the research window scrolls its topics a row at a time.
  TEST_METHOD(LaysOutTheProductionAndResearchWindows)
  {
    Outpost::Snapshot newest = DesignerSnapshot();
    Outpost::Hud::Content content;
    content.production = Outpost::Hud::DescribeProduction(newest, &newest.entities.front());
    Outpost::Hud::ResearchPanel research{.lab = "RESEARCH LAB", .hasLab = true, .ore = 500};
    for (std::uint32_t i = 1; i <= 12; ++i)
    {
      research.topics.push_back({.name = std::format("Topic {}", i),
                                 .cost = 100,
                                 .action = {.kind = Outpost::Hud::ActionKind::Research, .topic = Outpost::ResearchTopicId{i}},
                                 .enabled = i != 2});
    }
    content.laboratory = research;
    Outpost::Designer designer;
    designer.Update(newest);
    content.designer = DesignerOf(newest, designer);
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(size_t{3}, layout.windows.size());
    const Outpost::Hud::Window& production = layout.windows[0];
    const Outpost::Hud::Window& lab = layout.windows[1];
    const Outpost::Hud::Window& designerWindow = layout.windows[2];
    Assert::IsTrue(production.kind == Outpost::WindowKind::Production && lab.kind == Outpost::WindowKind::Research);
    Assert::IsTrue(production.frame.left + production.frame.width <= lab.frame.left, L"side by side");
    Assert::IsTrue(lab.frame.left + lab.frame.width <= designerWindow.frame.left, L"clear of the designer");
    Assert::IsTrue(production.frame.top + production.frame.height < 1080.0f && lab.frame.top + lab.frame.height < 1080.0f);

    const auto count = [&layout](std::size_t _layer, Outpost::Hud::ActionKind _kind)
    {
      const Outpost::Hud::Span span = layout.ActionsOf(_layer);
      return static_cast<size_t>(std::count_if(layout.actions.begin() + static_cast<std::ptrdiff_t>(span.first),
                                               layout.actions.begin() + static_cast<std::ptrdiff_t>(span.end),
                                               [_kind](const auto& _entry) { return _entry.second.kind == _kind; }));
    };
    Assert::AreEqual(size_t{1}, count(1, Outpost::Hud::ActionKind::Queue));
    Assert::AreEqual(size_t{9}, count(2, Outpost::Hud::ActionKind::Research), L"ten shown, one of them dim");
    Assert::AreEqual(size_t{1}, count(2, Outpost::Hud::ActionKind::NextTopics));
    Assert::AreEqual(size_t{0}, count(2, Outpost::Hud::ActionKind::PreviousTopics), L"at the first");

    // A row at a time, never past where the last row shows.
    Assert::AreEqual(size_t{2}, Outpost::Hud::StepTopics(0, 1, 12));
    Assert::AreEqual(size_t{2}, Outpost::Hud::StepTopics(2, 1, 12));
    Assert::AreEqual(size_t{0}, Outpost::Hud::StepTopics(2, -3, 12));
    Assert::AreEqual(size_t{0}, Outpost::Hud::StepTopics(0, 1, 10), L"all fit");
    Assert::AreEqual(size_t{2}, Outpost::Hud::StepTopics(7, 0, 12), L"kept in range");
  }

  // ADR-040: an asteroid field is only in the way, so its mark is darker than an ore asteroid's, though it is the larger.
  // ADR-043: an ore asteroid is in Ore's gold.
  TEST_METHOD(DrawsFieldsDarkerThanOreAsteroidsOnTheMinimap)
  {
    Outpost::Hud::Content content{.ore = 0, .selection = {}, .mapSizeMeters = 2000.0f};
    content.marks = {
      {.position = {.xMeters = -500.0f, .zMeters = 0.0f},
       .radiusMeters = 45.0f,
       .side = Outpost::Hud::Side::Neutral,
       .kind = Outpost::EntityKind::Asteroid},
      {.position = {.xMeters = 500.0f, .zMeters = 0.0f},
       .radiusMeters = 150.0f,
       .side = Outpost::Hud::Side::Neutral,
       .kind = Outpost::EntityKind::AsteroidField},
    };
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);

    // With no fog and no view, the marks are the last panels, in order.
    Assert::IsTrue(layout.panels.size() >= 2);
    const Outpost::Hud::Rect& ore = layout.panels[layout.panels.size() - 2];
    const Outpost::Hud::Rect& field = layout.panels.back();
    Assert::IsTrue(field.width > ore.width, L"the field's square is the larger");
    const auto luminance = [](const DirectX::XMFLOAT4& _color)
    { return (0.2126f * _color.x) + (0.7152f * _color.y) + (0.0722f * _color.z); };
    Assert::IsTrue(luminance(field.color) < luminance(ore.color));
    Assert::IsTrue(ore.color.x > ore.color.y && ore.color.y > ore.color.z, L"gold");
  }

  // ADR-046: the Ore's diamond and figure start at the panel's left whatever the figure, and the income keeps to its right.
  TEST_METHOD(KeepsTheOreDiamondStillAsTheFigureChanges)
  {
    const auto oreAt = [](std::int32_t _ore)
    {
      const Outpost::Hud::Layout layout = Lay({.ore = _ore, .oreIncomeHundredthsPerSecond = 2700}, 1920, 1080);
      const auto diamond = std::ranges::find(layout.sprites, Outpost::Hud::Sprite::OreMark, &Outpost::Hud::SpriteMark::sprite);
      const auto figure = std::ranges::find(layout.texts, Outpost::WithThousands(_ore), &Outpost::Hud::Text::text);
      const auto income = std::ranges::find(layout.texts, std::string("+27/s"), &Outpost::Hud::Text::text);
      Assert::IsTrue(diamond != layout.sprites.end() && figure != layout.texts.end() && income != layout.texts.end());
      return std::array<float, 3>{diamond->area.left, figure->left, income->left};
    };
    const std::array<float, 3> oneDigit = oreAt(5);
    const std::array<float, 3> eightDigits = oreAt(12345678);
    Assert::AreEqual(oneDigit[0], eightDigits[0], 0.01f, L"the diamond");
    Assert::AreEqual(oneDigit[1], eightDigits[1], 0.01f, L"the figure");
    Assert::AreEqual(oneDigit[2], eightDigits[2], 0.01f, L"the income");
    Assert::IsTrue(oneDigit[0] < 40.0f, L"at the panel's left");
  }

  // ADR-046: the selection's hit points show as a bar under its lines, as long as the share left, red when low.
  TEST_METHOD(DrawsTheSelectionsHealthAsABar)
  {
    const Outpost::Hud::Content healthy{.ore = 0, .selection = {"Shipyard", "Hit points 3,000 / 3,000"}};
    Outpost::Hud::Content hurt = healthy;
    hurt.selectionHealth = 0.2f;
    const Outpost::Hud::Layout without = Lay(healthy, 1920, 1080);
    const Outpost::Hud::Layout with = Lay(hurt, 1920, 1080);
    const Outpost::Hud::Rect& panel = with.panels[1];
    Assert::IsTrue(panel.height > without.panels[1].height, L"the bar is under the lines");
    Assert::AreEqual(without.panels[1].top + without.panels[1].height, panel.top + panel.height, 0.01f, L"anchored to the bottom");
    const Outpost::Hud::Rect& track = with.panels[2];
    const Outpost::Hud::Rect& bar = with.panels[3];
    Assert::AreEqual(0.2f * track.width, bar.width, 0.01f);
    Assert::IsTrue(bar.color.x > bar.color.y, L"red");
    Assert::IsTrue(track.top > panel.top + (panel.height / 2.0f) && track.top + track.height < panel.top + panel.height);
  }

  // ADR-046: an ore asteroid's mark is the largest of the minimap's smallest marks, and a rig's own mark shows over it.
  TEST_METHOD(DrawsARigsMarkOverItsAsteroid)
  {
    Outpost::Hud::Content content{.ore = 0, .selection = {}, .mapSizeMeters = 5000.0f};
    const Outpost::PlanePosition at{.xMeters = 400.0f, .zMeters = 400.0f};
    content.marks = {{.position = at, .radiusMeters = 45.0f, .side = Outpost::Hud::Side::Own, .kind = Outpost::EntityKind::Structure},
                     {.position = at, .radiusMeters = 45.0f, .side = Outpost::Hud::Side::Neutral, .kind = Outpost::EntityKind::Asteroid}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const Outpost::Hud::Rect& ore = layout.panels[layout.panels.size() - 2];
    const Outpost::Hud::Rect& rig = layout.panels.back();
    Assert::IsTrue(ore.color.x > ore.color.z && rig.color.z > rig.color.x, L"the gold ore first, the blue rig over it");
    Assert::AreEqual(8.0f, ore.width, 0.01f);
    Assert::IsTrue(rig.width < ore.width, L"the asteroid's gold shows round the rig");
  }

  // ADR-043: the HUD's panels are in the windows' look, a window's body with a bracket at each corner.
  TEST_METHOD(LaysTheHudOutInTheWindowsLook)
  {
    const Outpost::Hud::Content content{.ore = 0,
                                        .selection = {"Shipyard"},
                                        .buttons = {{.label = "Production"}},
                                        .research = "Researching Hull Plating, 25%",
                                        .mapSizeMeters = 2000.0f};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const Outpost::Hud::Span sprites = layout.SpritesOf(0);
    const auto corners = std::count_if(layout.sprites.begin() + static_cast<std::ptrdiff_t>(sprites.first),
                                       layout.sprites.begin() + static_cast<std::ptrdiff_t>(sprites.end),
                                       [](const Outpost::Hud::SpriteMark& _mark) { return _mark.sprite == Outpost::Hud::Sprite::Corner; });
    Assert::AreEqual(std::ptrdiff_t{20}, corners, L"four for each of the Ore, the research, the selection, the buttons and the minimap");
  }

  // ADR-043: Ore is written one way, as Ore's diamond and the figure grouped in thousands: the stockpile, a button's cost,
  // and a window's Ore and its cards' costs.
  TEST_METHOD(WritesOreOneWay)
  {
    Outpost::Hud::Content content{.ore = 12345,
                                  .buttons = {{.label = "Shipyard|1500", .action = {.kind = Outpost::Hud::ActionKind::Build}}}};
    content.production =
      Outpost::Hud::ProductionPanel{.producer = "SHIPYARD 01",
                                    .hasProducer = true,
                                    .ore = 12345,
                                    .options = {{.name = "Swarm", .cost = 2500, .action = {.kind = Outpost::Hud::ActionKind::Queue}}}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const auto has = [&layout](std::string_view _text)
    { return std::ranges::any_of(layout.texts, [_text](const Outpost::Hud::Text& _line) { return _line.text == _text; }); };
    Assert::IsTrue(has("12,345") && has("1,500") && has("2,500"));
    Assert::IsFalse(has("12345") || has("Ore") || has("1500"));
    Assert::AreEqual(std::ptrdiff_t{4},
                     std::ranges::count(layout.sprites, Outpost::Hud::Sprite::OreMark, &Outpost::Hud::SpriteMark::sprite),
                     L"a diamond for the stockpile, the button, the window's Ore and the card");
  }

  // ADR-043: the production window's cards stay where they are as its queue grows, so that a card can be clicked again and
  // again. The queue stands under them, a row for each job and none empty, and the window grows at its foot.
  TEST_METHOD(KeepsTheCardsStillAsTheQueueGrows)
  {
    Outpost::Snapshot newest = DesignerSnapshot();
    Outpost::EntityView& yard = newest.entities.front();
    const auto lay = [&newest, &yard](std::size_t _jobs)
    {
      yard.queue.assign(_jobs, Outpost::JobView{.design = SWARM});
      Outpost::Hud::Content content;
      content.production = Outpost::Hud::DescribeProduction(newest, &yard);
      return Lay(content, 1920, 1080);
    };
    const Outpost::Hud::Layout empty = lay(0);
    const Outpost::Hud::Layout busy = lay(Outpost::QUEUE_LIMIT - 1);
    const auto card = [](const Outpost::Hud::Layout& _layout)
    {
      const auto queue =
        std::ranges::find_if(_layout.actions, [](const auto& _entry) { return _entry.second.kind == Outpost::Hud::ActionKind::Queue; });
      return queue != _layout.actions.end() ? queue->first : Outpost::Hud::Rect{};
    };
    Assert::IsTrue(card(busy).width > 0.0f);
    Assert::AreEqual(card(empty).top, card(busy).top, 0.01f);
    Assert::AreEqual(card(empty).left, card(busy).left, 0.01f);

    // The card's name, and one row for each job.
    const auto names = [](const Outpost::Hud::Layout& _layout)
    { return std::ranges::count(_layout.texts, std::string("Swarm"), &Outpost::Hud::Text::text); };
    Assert::AreEqual(std::ptrdiff_t{1}, names(empty));
    Assert::AreEqual(static_cast<std::ptrdiff_t>(Outpost::QUEUE_LIMIT), names(busy));
    const auto lastRow = std::ranges::find_last(busy.texts, std::string("Swarm"), &Outpost::Hud::Text::text);
    Assert::IsTrue(lastRow.begin()->top > card(busy).top + card(busy).height, L"the queue is under the cards");
    Assert::IsTrue(busy.windows.front().frame.height > empty.windows.front().frame.height);
    Assert::AreEqual(empty.windows.front().frame.top, busy.windows.front().frame.top, 0.01f);
  }

  // ADR-043: the selection panel is as wide as its longest line, within bounds, and stays centered.
  TEST_METHOD(FitsTheSelectionPanelToItsLines)
  {
    const auto panelOf = [](std::vector<std::string> _lines)
    {
      const Outpost::Hud::Layout layout = Lay({.ore = 0, .selection = std::move(_lines)}, 1920, 1080);
      return layout.panels[1];
    };
    const Outpost::Hud::Rect narrow = panelOf({"Shipyard", "Hit points 3,000 / 3,000"});
    const Outpost::Hud::Rect wide = panelOf({"12 ships", "6 x Medium+Fusion+Missile Rack Mark II", "Hit points 9,000 / 9,000"});
    const Outpost::Hud::Rect widest = panelOf({"40 ships", std::string(80, 'x')});
    Assert::IsTrue(narrow.width < wide.width && wide.width < widest.width);
    Assert::AreEqual(280.0f, narrow.width, 0.01f, L"no narrower than this");
    Assert::AreEqual(560.0f, widest.width, 0.01f, L"no wider than it was");
    Assert::AreEqual(960.0f, wide.left + (wide.width / 2.0f), 0.01f);
    Assert::IsTrue(wide.height > narrow.height);
  }

  // ADR-024, ADR-052: under fog of war the minimap draws the fog over its marks, as one panel over the whole minimap that the
  // client fills from the fog's texture, after the marks; without fog there is no such panel.
  TEST_METHOD(ShadesTheMinimapUnderFog)
  {
    Outpost::Hud::Content content{.ore = 0, .selection = {}, .mapSizeMeters = 2000.0f};
    content.marks.push_back({.position = {}, .radiusMeters = 8.0f, .side = Outpost::Hud::Side::Own, .kind = Outpost::EntityKind::Ship});
    const auto isFog = [](const Outpost::Hud::Rect& _panel) { return _panel.fill == Outpost::Hud::Fill::Fog; };
    Assert::IsFalse(std::ranges::any_of(Lay(content, 1920, 1080).panels, isFog), L"no fog without fog of war");

    content.fog = true;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(size_t{1}, static_cast<size_t>(std::ranges::count_if(layout.panels, isFog)));
    const auto fog = std::ranges::find_if(layout.panels, isFog);
    Assert::AreEqual(layout.minimap.left, fog->left);
    Assert::AreEqual(layout.minimap.top, fog->top);
    Assert::AreEqual(layout.minimap.width, fog->width);
    Assert::AreEqual(layout.minimap.height, fog->height);
    Assert::AreEqual(1.0f, fog->color.w, L"each point as opaque as its shade");
    const DirectX::XMFLOAT2 mark = layout.MinimapPixelOf({});
    const auto markPanel = std::ranges::find_if(layout.panels, [&](const Outpost::Hud::Rect& _panel)
                                                { return !isFog(_panel) && _panel.width < 20.0f && _panel.Contains(mark.x, mark.y); });
    Assert::IsTrue(markPanel != layout.panels.end() && markPanel < fog, L"the fog is drawn over the marks");
  }

  // A press on a panel belongs to the HUD.
  TEST_METHOD(CoversItsPanels)
  {
    const Outpost::Hud::Layout layout = Lay({.ore = 0, .selection = {}}, 1920, 1080);
    Assert::IsTrue(layout.Covers(20.0f, 20.0f));
    Assert::IsFalse(layout.Covers(960.0f, 540.0f));
  }
  TEST_METHOD(WritesALengthAsMinutesAndSeconds)
  {
    Assert::AreEqual(std::string("0:00"), Outpost::MinutesAndSeconds(0));
    Assert::AreEqual(std::string("0:59"), Outpost::MinutesAndSeconds(59));
    Assert::AreEqual(std::string("6:13"), Outpost::MinutesAndSeconds(373));
    Assert::AreEqual(std::string("59:59"), Outpost::MinutesAndSeconds(3599));
    Assert::AreEqual(std::string("1:02:03"), Outpost::MinutesAndSeconds(3723));
  }

  // Task 6.2: once the match is over the player reads whether they won, and how long it took; nothing shows before.
  TEST_METHOD(DescribesHowTheMatchEnded)
  {
    Outpost::Snapshot newest = Newest();
    Assert::IsFalse(Outpost::Hud::DescribeOutcome(newest, 20).has_value());

    newest.matchOver = true;
    newest.matchEndedTick = 7460;
    newest.winner = PLAYER;
    const auto outcome = [&newest] { return Outpost::Hud::DescribeOutcome(newest, 20).value_or(Outpost::Hud::Outcome{}); };
    Assert::AreEqual(std::string("Victory"), outcome().title);
    Assert::AreEqual(std::string("Match length 6:13"), outcome().detail);

    newest.winner = Outpost::PlayerId{2};
    Assert::AreEqual(std::string("Defeat"), outcome().title);
    newest.winner = {};
    Assert::AreEqual(std::string("Draw"), outcome().title);

    // Phase 2 design §8: a domination says so.
    newest.winner = PLAYER;
    newest.ending = Outpost::MatchEnding::Domination;
    Assert::AreEqual(std::string("By domination. Match length 6:13"), outcome().detail);
  }

  // ADR-059: a selection on a standing order says so.
  TEST_METHOD(SaysWhenTheSelectionStands)
  {
    Outpost::EntityView holding = Ship(9, SWARM, 30000, 30000);
    holding.standing = Outpost::StandingOrder::HoldSector;
    const std::vector<Outpost::EntityView> entities{holding};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(Newest(), entities, std::vector<Outpost::EntityId>{Outpost::EntityId{9}});
    Assert::AreEqual(std::string("Holding a sector"), content.selection.back());
  }

  // ADR-059: the alerts under the territory, the newest in the warning's color, and a mark at each on the minimap.
  TEST_METHOD(ListsTheAlerts)
  {
    Outpost::Hud::Content content{.ore = 0, .selection = {}, .mapSizeMeters = 2000.0f};
    content.alerts = {{"Relay suppressed: South", {.xMeters = 0.0f, .zMeters = -500.0f}}, {"Enemy ships in West", {}}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const auto newest = std::ranges::find(layout.texts, std::string("Relay suppressed: South"), &Outpost::Hud::Text::text);
    const auto older = std::ranges::find(layout.texts, std::string("Enemy ships in West"), &Outpost::Hud::Text::text);
    Assert::IsTrue(newest != layout.texts.end() && older != layout.texts.end());
    Assert::IsTrue(newest->top < older->top, L"newest first");
    Assert::IsTrue(newest->color.x > older->color.x && newest->color.z < older->color.z, L"the newest in the warning's color");
    const DirectX::XMFLOAT2 at = layout.MinimapPixelOf({.xMeters = 0.0f, .zMeters = -500.0f});
    Assert::IsTrue(
      std::ranges::any_of(layout.panels, [&](const Outpost::Hud::Rect& _panel)
                          { return _panel.left < at.x && _panel.left + _panel.width > at.x && _panel.top < at.y && _panel.height < 3.0f; }),
      L"a mark round the alert's place");
  }

  // ADR-057: under the nodes, each side's tickets, the player's in its color and the enemy's in theirs.
  TEST_METHOD(ShowsTheTickets)
  {
    Outpost::Snapshot newest = Newest();
    newest.sectors = {{.id = 1, .maxXMeters = 100.0f, .maxZMeters = 100.0f, .holder = PLAYER}};
    newest.tickets = {{.player = PLAYER, .tickets = 1000}, {.player = Outpost::PlayerId{2}, .tickets = 870}};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, {}, {});
    Assert::IsTrue(content.territory.has_value());
    const Outpost::Hud::Territory territory = content.territory.value_or(Outpost::Hud::Territory{});
    Assert::IsTrue(territory.ownTickets == 1000 && territory.enemyTickets == 870);
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const auto text = [&layout](std::string_view _text) { return std::ranges::find(layout.texts, _text, &Outpost::Hud::Text::text); };
    Assert::IsTrue(text("Tickets") != layout.texts.end());
    Assert::IsTrue(text("1,000 : ") != layout.texts.end() && text("870") != layout.texts.end());
    Assert::IsTrue(text("870")->left > text("1,000 : ")->left, L"the enemy's figure stands at the right");
    Assert::IsTrue(text("Tickets")->top > text("Nodes of 1")->top);
  }

  // The banner sits at the top, over the world that runs on, and its button takes the player back to the menu.
  TEST_METHOD(LaysOutTheBannerWithTheWayBack)
  {
    const Outpost::Hud::Content content{.ore = 0, .outcome = Outpost::Hud::Outcome{.title = "Defeat", .detail = "Match length 6:13"}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "Defeat"; }));
    Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "Match length 6:13"; }));
    Assert::AreEqual(size_t{1}, layout.actions.size());
    const auto& [area, action] = layout.actions.front();
    Assert::IsTrue(action.kind == Outpost::Hud::ActionKind::BackToMenu);
    Assert::IsTrue(area.top < 200.0f && area.left < 960.0f && area.left + area.width > 960.0f, L"top middle");
    Assert::IsTrue(layout.Covers(area.left + 5.0f, area.top + 5.0f));
    Assert::IsFalse(layout.Covers(960.0f, 540.0f), L"the middle of the screen stays on the world");
  }

  // Task 6.2: the main menu starts a skirmish or quits, from the middle of the screen at any size.
  TEST_METHOD(LaysOutTheMenu)
  {
    for (const auto& [width, height] : std::array<std::pair<std::uint32_t, std::uint32_t>, 2>{{{1920, 1080}, {1280, 720}}})
    {
      const Outpost::Hud::Layout layout = LayMenu(width, height);
      Assert::AreEqual(size_t{2}, layout.actions.size());
      Assert::IsTrue(layout.actions[0].second.kind == Outpost::Hud::ActionKind::StartSkirmish);
      Assert::IsTrue(layout.actions[1].second.kind == Outpost::Hud::ActionKind::Quit);
      const Outpost::Hud::Rect& start = layout.actions[0].first;
      Assert::IsTrue(start.left < static_cast<float>(width) / 2.0f && start.left + start.width > static_cast<float>(width) / 2.0f);
      Assert::IsTrue(layout.actions[1].first.top > start.top + start.height, L"Quit under Start skirmish");
      Assert::IsTrue(layout.ActionAt(start.left + 5.0f, start.top + 5.0f) ==
                     Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::StartSkirmish});
      Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "Outpost Commander"; }));
      Assert::IsFalse(layout.minimap.width > 0.0f);
    }
  }
  // Task 14.1: at either size, with the longest content, every text ends inside the smallest panel it starts in, measured
  // with the fonts it is drawn in. Every offender is named, not only the first.
  TEST_METHOD(KeepsEveryTextInsideItsPanel)
  {
    std::wstring offenders;
    for (const auto& [width, height] : std::array<std::pair<std::uint32_t, std::uint32_t>, 2>{{{1920, 1080}, {1280, 720}}})
    {
      const std::span<const Neuron::GlyphAtlas::Font> fonts = FontsFor(width, height);
      for (const auto& [name, layout] : LongestLayouts(width, height))
      {
        for (std::size_t layer = 0; layer < layout.LayerCount(); ++layer)
        {
          const Outpost::Hud::Span texts = layout.TextsOf(layer);
          const Outpost::Hud::Span panels = layout.PanelsOf(layer);
          for (std::size_t t = texts.first; t < texts.end; ++t)
          {
            const Outpost::Hud::Text& text = layout.texts[t];
            if (text.text.empty())
              continue;
            const Outpost::Hud::Rect box = LineBoxOf(text, fonts);
            const Outpost::Hud::Rect* smallest = nullptr;
            for (std::size_t p = panels.first; p < panels.end; ++p)
            {
              const Outpost::Hud::Rect& panel = layout.panels[p];
              if (panel.Contains(box.left + 0.5f, box.top + 0.5f) &&
                  (smallest == nullptr || panel.width * panel.height < smallest->width * smallest->height))
                smallest = &panel;
            }
            if (smallest == nullptr)
              offenders += std::format(L"{} starts in no panel\n", Named(name, width, text));
            else if (const float past = box.left + box.width - smallest->left - smallest->width; past > 0.5f)
              offenders += std::format(L"{} runs {} px past its panel\n", Named(name, width, text), past);
          }
        }
      }
    }
    Assert::IsTrue(offenders.empty(), offenders.c_str());
  }

  // Task 14.1: at either size, with the longest content, no two texts' line boxes meet within a layer. Every pair that
  // meets is named, not only the first.
  TEST_METHOD(OverlapsNoTwoTexts)
  {
    std::wstring offenders;
    for (const auto& [width, height] : std::array<std::pair<std::uint32_t, std::uint32_t>, 2>{{{1920, 1080}, {1280, 720}}})
    {
      const std::span<const Neuron::GlyphAtlas::Font> fonts = FontsFor(width, height);
      for (const auto& [name, layout] : LongestLayouts(width, height))
      {
        for (std::size_t layer = 0; layer < layout.LayerCount(); ++layer)
        {
          const Outpost::Hud::Span texts = layout.TextsOf(layer);
          for (std::size_t a = texts.first; a < texts.end; ++a)
          {
            const Outpost::Hud::Rect first = LineBoxOf(layout.texts[a], fonts);
            for (std::size_t b = a + 1; b < texts.end; ++b)
            {
              const Outpost::Hud::Rect second = LineBoxOf(layout.texts[b], fonts);
              const bool meet = first.width > 0.0f && second.width > 0.0f && first.left < second.left + second.width &&
                                second.left < first.left + first.width && first.top < second.top + second.height &&
                                second.top < first.top + first.height;
              if (meet)
              {
                offenders += std::format(L"{} meets \"{}\"\n", Named(name, width, layout.texts[a]),
                                         std::wstring(winrt::to_hstring(layout.texts[b].text)));
              }
            }
          }
        }
      }
    }
    Assert::IsTrue(offenders.empty(), offenders.c_str());
  }
};
} // namespace GameAppTests