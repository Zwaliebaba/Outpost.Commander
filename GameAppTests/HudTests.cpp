#include "pch.h"

#include <algorithm>

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

Outpost::EntityView Ship(std::uint32_t _id, Outpost::DesignId _design, std::int32_t _hitPointsHundredths, std::int32_t _maxHundredths)
{
  return {.id = Outpost::EntityId{_id}, .owner = PLAYER, .design = _design, .hitPointsHundredths = _hitPointsHundredths,
          .maxHitPointsHundredths = _maxHundredths};
}
} // namespace

TEST_CLASS(HudTests)
{
public:
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
    Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text)
    {
      return _text.text == "12,345";
    }));
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
    Assert::IsTrue(
      content.buttons[2].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Build,
                                                        .structure = Outpost::StructureKind::MiningRig});

    entities.push_back({.id = Outpost::EntityId{20}, .kind = Outpost::EntityKind::Structure, .owner = PLAYER,
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
    Outpost::EntityView yard{.id = Outpost::EntityId{30}, .kind = Outpost::EntityKind::Structure, .owner = PLAYER,
                             .structure = Outpost::StructureKind::Shipyard, .hitPointsHundredths = 250000, .maxHitPointsHundredths = 250000,
                             .queue = {{.design = SWARM}, {.design = LINE}}, .jobPermille = 455};
    const std::vector<Outpost::EntityId> selected{yard.id};
    Outpost::Hud::Content content = Outpost::Hud::Describe(newest, std::vector{yard}, selected);
    const std::vector<std::string> expected{"Shipyard", "Hit points 2,500 / 2,500", "1. Small+Ion+Mass Driver, 45%", "2. Medium+Ion+Lance"};
    Assert::IsTrue(content.selection == expected);
    Assert::AreEqual(size_t{2}, content.buttons.size());
    Assert::AreEqual(std::string("Small+Ion+Mass Driver|87"), content.buttons[0].label);
    Assert::IsTrue(
      content.buttons[0].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Queue, .producer = yard.id, .design = SWARM});
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
    constexpr Outpost::Hud::Action build{.kind = Outpost::Hud::ActionKind::Build, .structure = Outpost::StructureKind::Shipyard};
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

    const Outpost::PlanePosition center = layout.MapPointAt(map.left + (map.width / 2.0f), map.top + (map.height / 2.0f)).value_or(
      Outpost::PlanePosition{1e9f, 1e9f});
    Assert::AreEqual(0.0f, center.xMeters, 0.5f);
    Assert::AreEqual(0.0f, center.zMeters, 0.5f);
    const DirectX::XMFLOAT2 northEast = layout.MinimapPixelOf({1000.0f, 1000.0f});
    Assert::AreEqual(map.left + map.width, northEast.x, 0.01f);
    Assert::AreEqual(map.top, northEast.y, 0.01f);
    Assert::IsFalse(layout.MapPointAt(map.left - 5.0f, map.top).has_value());
  }

  // A press on a panel belongs to the HUD.
  TEST_METHOD(CoversItsPanels)
  {
    const Outpost::Hud::Layout layout = Outpost::Hud::Lay({.ore = 0, .selection = {}}, 1920, 1080);
    Assert::IsTrue(layout.Covers(20.0f, 20.0f));
    Assert::IsFalse(layout.Covers(960.0f, 540.0f));
  }
};
} // namespace GameAppTests