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

Outpost::Snapshot Newest()
{
  Outpost::Snapshot snapshot{.tick = 5, .player = PLAYER, .ore = 12345};
  snapshot.designs = {{.id = SWARM, .nameUtf8 = "Small+Ion+Mass Driver"}, {.id = LINE, .nameUtf8 = "Medium+Ion+Lance"}};
  return snapshot;
}

// A designer with a name field and one pick of each slot, and a button of the HUD's, for laying windows out.
Outpost::Hud::Content WithDesigner()
{
  Outpost::Hud::Content content;
  content.buttons = {{.label = "Shipyard|300", .action = {.kind = Outpost::Hud::ActionKind::Build}}};
  Outpost::Hud::DesignerPanel designer{.name = "Swarm"};
  designer.hulls = {{.label = "Small", .action = {.kind = Outpost::Hud::ActionKind::PickHull, .hull = Outpost::HullId{1}}}};
  content.designer = designer;
  return content;
}

Outpost::EntityView Ship(std::uint32_t _id, Outpost::DesignId _design, std::int32_t _hitPointsHundredths, std::int32_t _maxHundredths)
{
  return {.id = Outpost::EntityId{_id},
          .owner = PLAYER,
          .design = _design,
          .hitPointsHundredths = _hitPointsHundredths,
          .maxHitPointsHundredths = _maxHundredths};
}
} // namespace

TEST_CLASS(HudTests)
{
public:
  // ADR-030: the HUD names a font for every typeface and a sprite for every sprite, in their order. The HUD's text stays
  // in Segoe UI at its size; the figures take Cascadia Mono, or Consolas where it is not installed (gate H7).
  TEST_METHOD(NamesItsTypefacesAndSprites)
  {
    const std::vector<Neuron::FontDesc> typefaces = Outpost::Hud::Typefaces();
    Assert::AreEqual(static_cast<std::size_t>(Outpost::Hud::Typeface::LargeFigure) + 1, typefaces.size());
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
    Assert::IsTrue(Outpost::Hud::Lay(WithDesigner(), 1920, 1080, {}, &windows).windows.empty(), L"closed, it is not laid out");
    windows.Open(Outpost::WindowKind::Designer);
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay(WithDesigner(), 1920, 1080, {}, &windows);
    Assert::AreEqual(size_t{1}, layout.windows.size());
    const Outpost::Hud::Window& window = layout.windows.front();
    Assert::IsTrue(window.kind == Outpost::WindowKind::Designer);
    Assert::IsTrue(window.frame.left > 1920.0f - 700.0f && window.frame.top < 30.0f, L"top right");
    Assert::IsTrue(window.titleBar.Contains(window.frame.left + 10.0f, window.frame.top + 10.0f));
    Assert::IsTrue(window.closeBox.left >= window.titleBar.left + window.titleBar.width);
    Assert::IsTrue(window.titleBar.fill == Outpost::Hud::Fill::Hatched);
    const Outpost::Hud::Span sprites = layout.SpritesOf(1);
    Assert::AreEqual(size_t{4}, sprites.end - sprites.first);
    Assert::AreEqual(size_t{0}, layout.SpritesOf(0).end, L"the HUD has none");

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
    const Outpost::Hud::Layout hudOnly = Outpost::Hud::Lay(WithDesigner(), 1920, 1080, {}, &windows);
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
    const Outpost::Hud::Layout covered = Outpost::Hud::Lay(WithDesigner(), 1920, 1080, {}, &windows);
    const Outpost::Hud::Window& window = covered.windows.front();
    Assert::IsTrue(window.frame.Contains(buildX, buildY));
    Assert::AreEqual(size_t{1}, covered.LayerAt(buildX, buildY));
    const std::optional<Outpost::Hud::Action> under = covered.ActionAt(buildX, buildY);
    Assert::IsTrue(!under.has_value() || under->kind != Outpost::Hud::ActionKind::Build, L"the window's, not the HUD's");

    // Dragged off the bottom-right corner, it keeps its title bar and some of its width on the screen.
    windows.Grab(Outpost::WindowKind::Designer, {}, {});
    windows.Drag({.xUnits = 5000.0f, .yUnits = 5000.0f});
    windows.Release();
    const Outpost::Hud::Window farWindow = Outpost::Hud::Lay(WithDesigner(), 1920, 1080, {}, &windows).windows.front();
    Assert::AreEqual(1920.0f - Outpost::Hud::WINDOW_KEPT_ON_SCREEN_UNITS, farWindow.frame.left, 0.01f);
    Assert::AreEqual(1080.0f - Outpost::Hud::TITLE_BAR_UNITS, farWindow.frame.top, 0.01f);
    // And off the top-left one.
    windows.Grab(Outpost::WindowKind::Designer, {}, {});
    windows.Drag({.xUnits = -5000.0f, .yUnits = -5000.0f});
    windows.Release();
    const Outpost::Hud::Window nearWindow = Outpost::Hud::Lay(WithDesigner(), 1280, 720, {}, &windows).windows.front();
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
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay(content, 1920, 1080);
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
  }

  // Several ships: how many, how many of each design, most first, and their hit points together.
  TEST_METHOD(DescribesAGroupByDesign)
  {
    const std::vector<Outpost::EntityView> entities{Ship(1, LINE, 45000, 45000), Ship(2, SWARM, 10000, 19800), Ship(3, SWARM, 19800, 19800),
                                                    Ship(4, SWARM, 19800, 19800)};
    const std::vector<Outpost::EntityId> selected{Outpost::EntityId{1}, Outpost::EntityId{2}, Outpost::EntityId{3}, Outpost::EntityId{4}};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(Newest(), entities, selected);
    const std::vector<std::string> expected{"4 ships", "3 x Small+Ion+Mass Driver", "1 x Medium+Ion+Lance", "Hit points 946 / 1,044"};
    Assert::IsTrue(content.selection == expected);
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
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay(content, 1920, 1200);
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
      const Outpost::Hud::Layout layout = Outpost::Hud::Lay({.ore = 0, .oreIncomeHundredthsPerSecond = _hundredths}, 1920, 1080);
      return layout.texts[2].text;
    };
    Assert::AreEqual(std::string("+15/s"), incomeText(1500));
    Assert::AreEqual(std::string("+6.5/s"), incomeText(650));
    Assert::AreEqual(std::string("+0/s"), incomeText(0));
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

    // While a placement is armed, a hint says what a click does.
    content = Outpost::Hud::Describe(newest, entities, selected, Outpost::StructureKind::MiningRig);
    Assert::AreEqual(std::string("Placing Mining Rig: left-click to build, right-click to cancel"), content.hint);
  }

  // Task 4.5: a selected Shipyard shows its state and its queue, the front job's progress or its wait for Ore, and a
  // button per design, dim once the queue is full.
  TEST_METHOD(DescribesAStructureAndItsQueue)
  {
    Outpost::Snapshot newest = Newest();
    newest.designs[0].cost = 87;
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
    Outpost::Hud::Content content = Outpost::Hud::Describe(newest, std::vector{yard}, selected);
    const std::vector<std::string> expected{"Shipyard", "Hit points 2,500 / 2,500", "1. Small+Ion+Mass Driver, 45%", "2. Medium+Ion+Lance"};
    Assert::IsTrue(content.selection == expected);
    Assert::AreEqual(size_t{2}, content.buttons.size());
    Assert::AreEqual(std::string("Small+Ion+Mass Driver|87"), content.buttons[0].label);
    Assert::IsTrue(content.buttons[0].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Queue, .producer = yard.id, .design = SWARM});
    Assert::IsTrue(content.buttons[0].enabled);

    yard.jobPermille = 0;
    yard.queue.resize(Outpost::QUEUE_LIMIT, {.design = SWARM});
    content = Outpost::Hud::Describe(newest, std::vector{yard}, selected);
    Assert::AreEqual(std::string("1. Small+Ion+Mass Driver, waiting for Ore"), content.selection[2]);
    Assert::IsFalse(content.buttons[0].enabled, L"the queue is full");

    // Under construction, it says how far it has come, and offers nothing.
    yard.builtPermille = 600;
    yard.queue.clear();
    content = Outpost::Hud::Describe(newest, std::vector{yard}, selected);
    Assert::AreEqual(std::string("Under construction, 60%"), content.selection[1]);
    Assert::IsFalse(content.buttons[0].enabled);
  }

  // Task 4.5: an enabled button is a place to click, anchored to the bottom-right corner; a dim one is not.
  TEST_METHOD(LaysOutButtonsForClicks)
  {
    const Outpost::Hud::Action build{.kind = Outpost::Hud::ActionKind::Build, .structure = Outpost::StructureKind::Shipyard};
    const Outpost::Hud::Content content{
      .ore = 0, .buttons = {{.label = "Shipyard|300", .action = build}, {.label = "Research Lab|200", .action = build, .enabled = false}}};
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay(content, 1920, 1080);
    Assert::AreEqual(size_t{1}, layout.actions.size());
    const Outpost::Hud::Rect& area = layout.actions.front().first;
    Assert::IsTrue(area.left + area.width < 1920.0f && area.left > 1920.0f - 400.0f);
    Assert::IsTrue(layout.ActionAt(area.left + 5.0f, area.top + 5.0f) == build);
    Assert::IsFalse(layout.ActionAt(area.left + 5.0f, area.top + area.height + 10.0f).has_value());
    Assert::IsTrue(layout.Covers(area.left + 5.0f, area.top + 5.0f));
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
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay(content, 1920, 1080, view);
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

  // Task 5.1: a selected Research Lab shows its queue, and offers each topic not researched or queued yet with what it
  // does and what it costs, dim while its prerequisite is neither; the topic under way shows under the Ore.
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
    const std::vector<Outpost::EntityId> selected{lab.id};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, std::vector{lab}, selected);
    const std::vector<std::string> expected{"Research Lab", "Hit points 1,500 / 1,500", "1. Hull Plating, 25%",
                                            "Fusion Drive: Unlocks the Fusion drive", "Automated Shipyards: Shipyard build speed +25%"};
    Assert::IsTrue(content.selection == expected);
    Assert::AreEqual(size_t{2}, content.buttons.size());
    Assert::AreEqual(std::string("Fusion Drive|200"), content.buttons[0].label);
    Assert::IsTrue(content.buttons[0].enabled, L"its prerequisite is queued");
    Assert::IsTrue(content.buttons[1].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Research,
                                                                     .producer = lab.id,
                                                                     .topic = Outpost::ResearchTopicId{8}});
    Assert::AreEqual(std::string("Researching Hull Plating, 25%"), content.research);

    // Nothing selected, the research still shows under the Ore.
    const Outpost::Hud::Content unselected = Outpost::Hud::Describe(newest, std::vector{lab}, {});
    Assert::AreEqual(std::string("Researching Hull Plating, 25%"), unselected.research);
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay(unselected, 1920, 1080);
    Assert::IsTrue(
      std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "Researching Hull Plating, 25%"; }));
  }

  // Task 5.2: a selected built Shipyard of the player's shows the designer at the top right: a lit pick and a dim locked
  // component in each slot, the stats with damage per second against each hull, a name field to click, and the actions.
  TEST_METHOD(ShowsTheDesignerBesideAShipyard)
  {
    Outpost::Snapshot newest = Newest();
    newest.ore = 500;
    newest.hulls = {{.id = Outpost::HullId{1},
                     .nameUtf8 = "Small",
                     .hitPointsHundredths = 22000,
                     .armorHundredths = 200,
                     .speedMetersPerSecond = 60.0,
                     .cost = 32,
                     .buildSeconds = 10.0,
                     .available = true},
                    {.id = Outpost::HullId{3},
                     .nameUtf8 = "Large",
                     .hitPointsHundredths = 120000,
                     .armorHundredths = 1400,
                     .speedMetersPerSecond = 25.0,
                     .cost = 300,
                     .buildSeconds = 40.0,
                     .available = false}};
    newest.drives = {
      {.id = Outpost::DriveId{1}, .nameUtf8 = "Ion", .speedFactor = 1.3, .hitPointsFactor = 0.9, .cost = 20, .available = true}};
    newest.weapons = {{.id = Outpost::WeaponId{1},
                       .nameUtf8 = "Mass Driver",
                       .damageHundredths = 1400,
                       .fireIntervalSeconds = 0.4,
                       .rangeMeters = 120.0,
                       .cost = 35,
                       .available = true}};
    newest.designs = {{.id = SWARM,
                       .nameUtf8 = "Swarm",
                       .hull = Outpost::HullId{1},
                       .drive = Outpost::DriveId{1},
                       .weapon = Outpost::WeaponId{1},
                       .cost = 87}};
    newest.shipyardBuildSpeedFactor = 1.25;
    const Outpost::EntityView yard{
      .id = Outpost::EntityId{30}, .kind = Outpost::EntityKind::Structure, .owner = PLAYER, .structure = Outpost::StructureKind::Shipyard};
    Outpost::Designer designer;
    designer.Update(newest);
    const std::vector<Outpost::EntityId> selected{yard.id};
    Assert::IsFalse(Outpost::Hud::Describe(newest, std::vector{yard}, selected).designer.has_value(), L"no designer given");

    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, std::vector{yard}, selected, std::nullopt, &designer);
    Assert::IsTrue(content.designer.has_value());
    const Outpost::Hud::DesignerPanel panel = content.designer.value_or(Outpost::Hud::DesignerPanel{});
    Assert::AreEqual(std::string("Swarm"), panel.name);
    Assert::IsTrue(panel.hulls[0].selected && panel.hulls[0].enabled);
    Assert::IsFalse(panel.hulls[1].enabled, L"the Large hull is locked");
    const std::vector<std::string> summary{"Hit points 198   Armor 2   Speed 78 m/s", "Range 120 m   Cost 87   Build 8 s"};
    Assert::IsTrue(panel.summary == summary);
    const std::vector<std::string> perShip{"per ship", "30.0", "8.8"};
    Assert::IsTrue(panel.table[1] == perShip);
    const std::vector<std::string> perOre{"per 100 Ore", "34.5", "10.1"};
    Assert::IsTrue(panel.table[2] == perOre);
    Assert::AreEqual(std::string("Rename"), panel.actions[0].label);
    Assert::IsFalse(panel.actions[0].enabled, L"the name has not changed");
    Assert::IsTrue(panel.actions[1].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Queue, .producer = yard.id, .design = SWARM});
    Assert::IsTrue(panel.actions[1].enabled);

    const Outpost::Hud::Layout layout = Outpost::Hud::Lay(content, 1920, 1080);
    const auto actionArea = [&layout](Outpost::Hud::ActionKind _kind)
    { return std::ranges::find(layout.actions, _kind, [](const auto& _entry) { return _entry.second.kind; })->first; };
    const Outpost::Hud::Rect name = actionArea(Outpost::Hud::ActionKind::EditName);
    Assert::IsTrue(name.left > 1920.0f - 700.0f && name.top < 100.0f, L"top right");
    Assert::IsTrue(layout.ActionAt(name.left + 5.0f, name.top + 5.0f) == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::EditName});
    const Outpost::Hud::Rect pick = actionArea(Outpost::Hud::ActionKind::PickHull);
    Assert::IsTrue(pick.top > name.top);
    Assert::AreEqual(size_t{1},
                     static_cast<size_t>(std::ranges::count(layout.actions, Outpost::Hud::ActionKind::PickHull,
                                                            [](const auto& _entry) { return _entry.second.kind; })),
                     L"the locked hull is no place to click");
    Assert::IsTrue(layout.Covers(name.left + 5.0f, name.top + 5.0f));

    // Another player's Shipyard shows no designer.
    Outpost::EntityView theirs = yard;
    theirs.owner = Outpost::PlayerId{2};
    Assert::IsFalse(Outpost::Hud::Describe(newest, std::vector{theirs}, selected, std::nullopt, &designer).designer.has_value());

    // Picks that are no saved design yet: Queue saves them first, at their cost (ADR-023).
    newest.hulls[1].available = true;
    designer.PickHull(Outpost::HullId{3});
    const auto unsavedPanel = [&]()
    {
      return Outpost::Hud::Describe(newest, std::vector{yard}, selected, std::nullopt, &designer)
        .designer.value_or(Outpost::Hud::DesignerPanel{});
    };
    const Outpost::Hud::DesignerPanel unsaved = unsavedPanel();
    Assert::AreEqual(std::string("Save design"), unsaved.actions[0].label);
    Assert::AreEqual(std::string("Queue|355"), unsaved.actions[1].label);
    Assert::IsTrue(unsaved.actions[1].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::SaveAndQueue, .producer = yard.id});
    Assert::IsTrue(unsaved.actions[1].enabled);
    newest.ore = 354;
    Assert::IsFalse(unsavedPanel().actions[1].enabled, L"short of Ore");
  }

  // ADR-024: the minimap draws the fog over its marks, a rectangle for each run of one shade along a row, and nothing
  // where the player sees.
  TEST_METHOD(ShadesTheMinimapUnderFog)
  {
    Outpost::FogOfWar fog;
    fog.Reset(2000.0f);
    fog.Update(
      std::vector{Outpost::EntityView{.owner = PLAYER, .position = {.xMeters = -900.0f, .zMeters = -900.0f}, .sightMeters = 150.0f}},
      PLAYER);
    Outpost::Hud::Content content{.ore = 0, .selection = {}, .mapSizeMeters = 2000.0f};
    const size_t plain = Outpost::Hud::Lay(content, 1920, 1080).panels.size();
    content.fogShades.assign(fog.Shades().begin(), fog.Shades().end());
    content.fogCellsPerSide = fog.CellsPerSide();
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay(content, 1920, 1080);

    std::vector<Outpost::Hud::Rect> shades(layout.panels.begin() + static_cast<std::ptrdiff_t>(plain), layout.panels.end());
    Assert::IsTrue(shades.size() > 100 && shades.size() < 300, L"runs along each row, not a rectangle per cell");
    const auto shadeAt = [&](Outpost::PlanePosition _point)
    {
      const DirectX::XMFLOAT2 pixel = layout.MinimapPixelOf(_point);
      const auto over = std::ranges::find_if(shades, [&](const Outpost::Hud::Rect& _rect) { return _rect.Contains(pixel.x, pixel.y); });
      return over != shades.end() ? over->color.w : 0.0f;
    };
    Assert::AreEqual(0.0f, shadeAt({.xMeters = -900.0f, .zMeters = -900.0f}), L"seen: clear");
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, shadeAt({.xMeters = 900.0f, .zMeters = 900.0f}));
    Assert::AreEqual(Outpost::FogOfWar::NEVER_SEEN_SHADE, shadeAt({.xMeters = -900.0f, .zMeters = 900.0f}), L"the rows run up the minimap");
  }

  // A press on a panel belongs to the HUD.
  TEST_METHOD(CoversItsPanels)
  {
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay({.ore = 0, .selection = {}}, 1920, 1080);
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
  }

  // The banner sits at the top, over the world that runs on, and its button takes the player back to the menu.
  TEST_METHOD(LaysOutTheBannerWithTheWayBack)
  {
    const Outpost::Hud::Content content{.ore = 0, .outcome = Outpost::Hud::Outcome{.title = "Defeat", .detail = "Match length 6:13"}};
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay(content, 1920, 1080);
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
      const Outpost::Hud::Layout layout = Outpost::Hud::LayMenu(width, height);
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
};
} // namespace GameAppTests