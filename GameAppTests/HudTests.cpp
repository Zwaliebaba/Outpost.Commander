#include "pch.h"

#include <algorithm>
#include <array>
#include <limits>

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
std::span<const Neuron::GlyphAtlas::Font> FontsFor(std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor = 1.0f)
{
  static std::map<float, std::vector<Neuron::GlyphAtlas::Font>> g_fontsByScale;
  const float scale = Outpost::Hud::Scale(_widthPixels, _heightPixels, _factor);
  auto found = g_fontsByScale.find(scale);
  if (found == g_fontsByScale.end())
  {
    found =
      g_fontsByScale.emplace(scale, Neuron::RasterizeUiAtlas(Outpost::Hud::Typefaces(), Outpost::Hud::Sprites(), scale).glyphs.fonts).first;
  }
  return found->second;
}

Outpost::Hud::TextMetrics MetricsFor(std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor = 1.0f)
{
  return {FontsFor(_widthPixels, _heightPixels, _factor), Outpost::Hud::Scale(_widthPixels, _heightPixels, _factor)};
}

// The HUD and the menu laid out as GameClient lays them out, with the fonts at the back buffer's scale and the interface's
// own (ADR-070).
Outpost::Hud::Layout Lay(const Outpost::Hud::Content& _content, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                         std::span<const Outpost::PlanePosition> _view = {}, const Outpost::WindowManager* _windows = nullptr,
                         float _factor = 1.0f)
{
  return Outpost::Hud::Lay(_content, MetricsFor(_widthPixels, _heightPixels, _factor), _widthPixels, _heightPixels, _view, _windows,
                           _factor);
}

Outpost::Hud::Layout LayMenu(std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor = 1.0f,
                             const Outpost::Hud::MenuState& _state = {})
{
  return Outpost::Hud::LayMenu(MetricsFor(_widthPixels, _heightPixels, _factor), _widthPixels, _heightPixels, _factor, _state);
}

Outpost::Snapshot Newest()
{
  Outpost::Snapshot snapshot{.tick = 5, .player = PLAYER, .ore = 12345};
  snapshot.designs = {{.id = SWARM, .nameUtf8 = "Small+Ion+Mass Driver"}, {.id = LINE, .nameUtf8 = "Medium+Ion+Lance"}};
  return snapshot;
}

// A sector, North, that the player holds, with a warship of the player's in it and an enemy Relay, for the orders window.
Outpost::Snapshot OrdersSnapshot()
{
  Outpost::Snapshot snapshot = Newest();
  snapshot.sectors = {{.id = 1,
                       .nameUtf8 = "North",
                       .minXMeters = -500.0f,
                       .maxXMeters = 500.0f,
                       .minZMeters = -500.0f,
                       .maxZMeters = 500.0f,
                       .node = {.xMeters = 0.0f, .zMeters = 100.0f},
                       .holder = PLAYER}};
  snapshot.structureTypes = {{.structure = Outpost::StructureKind::Relay, .nameUtf8 = "Relay"}};
  snapshot.entities = {
    {.id = Outpost::EntityId{100}, .owner = PLAYER, .design = SWARM, .hitPointsHundredths = 100, .maxHitPointsHundredths = 100},
    {.id = Outpost::EntityId{200},
     .kind = Outpost::EntityKind::Structure,
     .owner = Outpost::PlayerId{2},
     .structure = Outpost::StructureKind::Relay,
     .position = {.xMeters = 0.0f, .zMeters = 100.0f}}};
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

// The longest content the game makes, with every window open (task 14.1), the Controls window among them (task 16.4), and
// the orders and away windows (Phase 5 design §11): six designs of the longest names, all selected,
// one of them loaded in the designer of five weapons, every part locked or every part unlocked; a full queue at a
// Shipyard and at the Lab; a page of topics, among them Relay Archives with both its prerequisites to do; a placement's
// hint, the status panel's lines, the alerts, the territory and the banner. The Ore is five figures, more than any match in
// the review banked. With _hovered, the designer previews the part under the pointer.
Outpost::Hud::Content LongestContent(bool _unlocked, std::optional<Outpost::Hud::Action> _hovered = std::nullopt)
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
    Outpost::Hud::Describe(newest, newest.entities, selected, Outpost::StructureKind::Shipyard, &designer, _hovered);
  // The status panel at its fullest (task UI3.2): the longest topic the tuning data has, waiting, with four more queued; the
  // Shipyards' counts at two figures, idle a chip; and the fleet against its cap. The column is as wide as the longest topic.
  content.status = {{.runs = {{.text = "Mass Driver Calibration"}, {.text = " \xC2\xB7 waiting for Ore"}},
                     .bar = Outpost::Hud::StatusBar{.share = 0.99f, .figure = "+4", .under = true}},
                    {.runs = {{.text = "Shipyards \xC2\xB7 BUILDING 12 \xC2\xB7 WAITING 12 \xC2\xB7 "}, {.text = "IDLE 12", .chip = true}}},
                    {.runs = {{.text = "Fleet"}}, .bar = Outpost::Hud::StatusBar{.share = 0.5f, .figure = "99 / 99"}}};
  content.researchNames = {"Mass Driver Calibration", "Reinforced Structures"};
  content.alerts = {{"Shipyard 05 is under attack", {}}, {"Mining Rig 12 destroyed", {}}, {"Relay 03 lost", {}}};
  content.territory = Outpost::Hud::Territory{.ownNodes = 12,
                                              .enemyNodes = 12,
                                              .nodes = 12,
                                              .cap = 12,
                                              .ownTickets = 10000,
                                              .enemyTickets = 10000,
                                              .drain = "You -9,999 a minute \xC2\xB7 out in 99:59",
                                              .drainWarns = true};
  content.outcome = Outpost::Hud::Outcome{.title = "Defeat", .detail = "Command Station destroyed \xC2\xB7 Match length 1:02:03"};
  // The tag at its widest: the longest address the tag holds, and the warning at its most seconds (ADR-086).
  content.connection =
    Outpost::Hud::Connection{.server = Outpost::Hud::ServerName("2001:db8:ffff:ffff:ffff:ffff:ffff:ffff", 65535), .silentSeconds = 999};
  content.buttons = {{.label = "Shipyard|300"},
                     {.label = "Research Lab|400", .enabled = false, .note = "ONE PER PLAYER"},
                     {.label = "Production", .key = "P"},
                     {.label = "Ship designer", .key = "D"},
                     {.label = "Research", .key = "R"}};

  Outpost::Hud::ProductionPanel production{.producer = "SHIPYARD 05", .hasProducer = true, .canStep = true, .ore = newest.ore};
  for (const Outpost::DesignView& design : newest.designs)
  {
    production.options.push_back({.name = design.nameUtf8,
                                  .detail = "M\xC2\xB7I\xC2\xB7L",
                                  .cost = design.cost,
                                  .time = "199.9 s",
                                  .strengths = {{.hull = "S", .rating = Outpost::Hud::Rating::Good},
                                                {.hull = "M", .rating = Outpost::Hud::Rating::Fair},
                                                {.hull = "L", .rating = Outpost::Hud::Rating::Poor}}});
  }
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
  content.controls = true;

  // The orders window with every order shown and more, and the form at its most rows; and the away window at its most lines.
  Outpost::Hud::OrdersPanel orders{.more = 99,
                                   .fires = "Fires at 23:55 on your clock, in 23:59:59",
                                   .give = {.label = "Give to 99 ships|", .enabled = false, .note = "NO ENEMY SEEN THERE"}};
  for (std::size_t order = 0; order < Outpost::Hud::ORDERS_SHOWN; ++order)
  {
    orders.scheduled.push_back("When the Relay in " + LongestName('S') + " is threatened, Mining Rig in " + LongestName('T') +
                               ", unless over 60 enemy CP");
  }
  for (const auto& [label, field] :
       std::array<std::pair<const char*, Outpost::OrderField>, Outpost::Hud::ORDER_ROWS>{{{"WHEN", Outpost::OrderField::Trigger},
                                                                                          {"HOUR", Outpost::OrderField::Hour},
                                                                                          {"MINUTE", Outpost::OrderField::Minute},
                                                                                          {"DO", Outpost::OrderField::Action},
                                                                                          {"WHERE", Outpost::OrderField::ActionSector},
                                                                                          {"TARGET", Outpost::OrderField::Target},
                                                                                          {"UNLESS", Outpost::OrderField::Condition}}})
    orders.rows.push_back({.label = label, .value = LongestName('V') + " (99 of 99)", .field = field});
  content.orders = orders;
  Outpost::Hud::AwayPanel away{.since = "Away for 999:59:59", .lines = {}};
  while (away.lines.size() < Outpost::Hud::AWAY_LINES)
    away.lines.push_back("Order held back: attack-move to " + LongestName('A') + " and " + LongestName('B'));
  content.away = away;
  return content;
}

// The interface's own scale steps the longest content's windows fit at this size (ADR-070), from the first.
std::vector<float> ReachableSteps(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  const Outpost::Snapshot newest = DesignerSnapshot(true, true);
  const std::array<float, 6>& steps = Outpost::Hud::INTERFACE_STEPS;
  std::vector<float> reachable{steps.front()};
  for (std::size_t step = 1; step < steps.size() && Outpost::Hud::WindowsFit(&newest, _widthPixels, _heightPixels, steps[step]); ++step)
    reachable.push_back(steps[step]);
  return reachable;
}

// What 14.1's and 14.2's tests lay out: the menu, and the longest content with its parts locked and unlocked, and with the
// Small hull previewed in place of the loaded design's Medium, which makes some figures better and some worse; at every
// step of the interface's own scale that the windows fit (ADR-070), each named with its step.
std::vector<std::pair<std::wstring, Outpost::Hud::Layout>> LongestLayouts(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  const Outpost::Hud::Action smallHull{.kind = Outpost::Hud::ActionKind::PickHull, .hull = Outpost::HullId{1}};
  std::vector<std::pair<std::wstring, Outpost::Hud::Layout>> layouts;
  for (const float factor : ReachableSteps(_widthPixels, _heightPixels))
  {
    const std::wstring step = std::format(L" at {}%", std::lround(factor * 100.0f));
    layouts.emplace_back(L"the menu" + step, LayMenu(_widthPixels, _heightPixels, factor));
    layouts.emplace_back(L"locked" + step, Lay(LongestContent(false), _widthPixels, _heightPixels, {}, nullptr, factor));
    layouts.emplace_back(L"unlocked" + step, Lay(LongestContent(true), _widthPixels, _heightPixels, {}, nullptr, factor));
    layouts.emplace_back(L"previewing" + step, Lay(LongestContent(true, smallHull), _widthPixels, _heightPixels, {}, nullptr, factor));
  }
  return layouts;
}

// The fonts a layout of LongestLayouts was set in: its step's, found by its font size.
std::span<const Neuron::GlyphAtlas::Font> FontsOf(const Outpost::Hud::Layout& _layout, std::uint32_t _widthPixels,
                                                  std::uint32_t _heightPixels)
{
  return FontsFor(_widthPixels, _heightPixels,
                  _layout.fontPixels / Outpost::Hud::FONT_UNITS / Outpost::Hud::Scale(_widthPixels, _heightPixels));
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

// WCAG's relative luminance of a linear color, which the render target encodes to sRGB, as the review computed it.
float LuminanceOf(const DirectX::XMFLOAT3& _color) noexcept
{
  return (0.2126f * _color.x) + (0.7152f * _color.y) + (0.0722f * _color.z);
}

// WCAG's contrast ratio between two linear colors.
float ContrastOf(const DirectX::XMFLOAT3& _first, const DirectX::XMFLOAT3& _second) noexcept
{
  const float first = LuminanceOf(_first) + 0.05f;
  const float second = LuminanceOf(_second) + 0.05f;
  return std::max(first, second) / std::min(first, second);
}

// What a text can stand on: the panels of its layer under its first pixel, laid over each other in the order they are
// drawn, at their alpha, over black. A hatched panel covers some pixels and not others, so it is taken both ways.
std::vector<DirectX::XMFLOAT3> GroundsOf(const Outpost::Hud::Layout& _layout, std::size_t _layer, const Outpost::Hud::Text& _text)
{
  const Outpost::Hud::Span panels = _layout.PanelsOf(_layer);
  const float x = std::round(_text.left) + 0.5f;
  const float y = std::round(_text.top) + 0.5f;
  std::vector<DirectX::XMFLOAT3> grounds{{0.0f, 0.0f, 0.0f}};
  for (std::size_t p = panels.first; p < panels.end; ++p)
  {
    const Outpost::Hud::Rect& panel = _layout.panels[p];
    if (!panel.Contains(x, y))
      continue;
    const float alpha = panel.color.w;
    const std::size_t count = grounds.size();
    for (std::size_t g = 0; g < count; ++g)
    {
      const DirectX::XMFLOAT3 under = grounds[g];
      const DirectX::XMFLOAT3 covered{(panel.color.x * alpha) + (under.x * (1.0f - alpha)),
                                      (panel.color.y * alpha) + (under.y * (1.0f - alpha)),
                                      (panel.color.z * alpha) + (under.z * (1.0f - alpha))};
      if (panel.fill == Outpost::Hud::Fill::Solid)
        grounds[g] = covered;
      else
        grounds.push_back(covered);
    }
  }
  return grounds;
}

// Whether _text is a warning chip (ADR-085 decision 1): its words dark, on a panel of the warning's color under its first
// pixel.
bool IsChip(const Outpost::Hud::Layout& _layout, const Outpost::Hud::Text& _text)
{
  const float x = std::round(_text.left) + 0.5f;
  const float y = std::round(_text.top) + 0.5f;
  const bool tag = std::ranges::any_of(_layout.panels,
                                       [&](const Outpost::Hud::Rect& _panel)
                                       {
                                         return _panel.color.x == 1.0f && _panel.color.y == 0.5f && _panel.color.z == 0.35f &&
                                                _panel.fill == Outpost::Hud::Fill::Solid && _panel.Contains(x, y);
                                       });
  return tag && LuminanceOf({_text.color.x, _text.color.y, _text.color.z}) < 0.05f;
}

// The text that says _words in _layout, which the test needs.
const Outpost::Hud::Text& TextOf(const Outpost::Hud::Layout& _layout, std::string_view _words)
{
  const auto found = std::ranges::find(_layout.texts, _words, &Outpost::Hud::Text::text);
  Assert::IsTrue(found != _layout.texts.end(), std::wstring(winrt::to_hstring(_words)).c_str());
  return *found;
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
  // ADR-062: the type scale, set in FaceUnits alone: labels and details at 13 units, figures at 14 in semibold, and the
  // name, title and large figures as the mockup has them.
  TEST_METHOD(NamesItsTypefacesAndSprites)
  {
    using Hud = Outpost::Hud;
    const std::vector<Neuron::FontDesc> typefaces = Hud::Typefaces();
    Assert::AreEqual(static_cast<std::size_t>(Hud::Typeface::Detail) + 1, typefaces.size());
    const Neuron::FontDesc& body = typefaces[static_cast<std::size_t>(Hud::Typeface::Body)];
    Assert::IsTrue(body.families == std::vector<std::wstring>{L"Segoe UI"});
    Assert::AreEqual(Hud::FONT_UNITS, body.emUnits);
    const Neuron::FontDesc& figure = typefaces[static_cast<std::size_t>(Hud::Typeface::Figure)];
    Assert::IsTrue(figure.families == std::vector<std::wstring>{L"Cascadia Mono", L"Consolas"});
    Assert::AreEqual(600, static_cast<int>(figure.weight), L"semibold");
    for (const Neuron::FontDesc& typeface : typefaces)
      Assert::IsTrue(!typeface.families.empty() && typeface.emUnits > 0.0f);
    const std::array<std::pair<Hud::Typeface, float>, 7> sizes{{{Hud::Typeface::Body, 20.0f},
                                                                {Hud::Typeface::Title, 22.0f},
                                                                {Hud::Typeface::Label, 13.0f},
                                                                {Hud::Typeface::Name, 16.0f},
                                                                {Hud::Typeface::Figure, 14.0f},
                                                                {Hud::Typeface::LargeFigure, 28.0f},
                                                                {Hud::Typeface::Detail, 13.0f}}};
    for (const auto& [face, units] : sizes)
    {
      Assert::AreEqual(units, Hud::FaceUnits(face));
      Assert::AreEqual(units, typefaces[static_cast<std::size_t>(face)].emUnits);
    }
    Assert::AreEqual(static_cast<std::size_t>(Hud::Sprite::Corner) + 1, Hud::Sprites().size());
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
    Assert::IsTrue(window.frame.left > 1920.0f - 850.0f && window.frame.top < 30.0f, L"top right");
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
    // Earning, so that the Ore panel holds no warning chip.
    Outpost::Snapshot newest = Newest();
    newest.oreIncomeHundredthsPerSecond = 650;
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, {}, {});
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

    const Outpost::Hud::Content content{.ore = 0, .oreIncomeHundredthsPerSecond = 650, .selection = {"1 ship", "Hit points 1 / 1"}};
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

  // An income of nothing is a warning chip (ADR-085 decision 1): no rig earns, so nothing the player spends comes back.
  TEST_METHOD(WarnsOfNoIncome)
  {
    const Outpost::Hud::Layout earning = Lay({.ore = 0, .oreIncomeHundredthsPerSecond = 650}, 1920, 1080);
    const Outpost::Hud::Text& income = TextOf(earning, "+6.5/s");
    Assert::IsTrue(income.color.z > income.color.x && !IsChip(earning, income), L"an income is in the figures' blue");
    const Outpost::Hud::Layout nothing = Lay({.ore = 0, .oreIncomeHundredthsPerSecond = 0}, 1920, 1080);
    Assert::IsTrue(IsChip(nothing, TextOf(nothing, "+0/s")), L"no income is a chip");
  }

  // ADR-085 decision 1: a warning is a chip, its words dark on a tag of the warning's color: an idle line's IDLE, an income
  // of nothing and the newest alert, each at 4.5:1 or more against its tag. Nothing else is, and the enemy's figures stay
  // plain text in their own color.
  TEST_METHOD(MarksEachWarningWithAChip)
  {
    Outpost::Hud::Content content{.ore = 0, .oreIncomeHundredthsPerSecond = 0};
    content.status = {{.runs = {{.text = "Research Lab "}, {.text = "IDLE", .chip = true}}}};
    content.alerts = {{"Enemy ships in South", {}}, {"Mining Rig lost: North", {}}};
    content.territory =
      Outpost::Hud::Territory{.ownNodes = 3, .enemyNodes = 2, .nodes = 25, .cap = 10, .ownTickets = 900, .enemyTickets = 950};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    for (const std::string_view words : {"IDLE", "+0/s", "Enemy ships in South"})
    {
      const Outpost::Hud::Text& chip = TextOf(layout, words);
      Assert::IsTrue(IsChip(layout, chip), std::wstring(winrt::to_hstring(words)).c_str());
      const std::vector<DirectX::XMFLOAT3> grounds = GroundsOf(layout, 0, chip);
      Assert::IsTrue(ContrastOf({chip.color.x, chip.color.y, chip.color.z}, grounds.back()) >= 4.5f, L"legible on its tag");
    }
    Assert::IsFalse(IsChip(layout, TextOf(layout, "Mining Rig lost: North")), L"an older alert is plain");
    Assert::IsFalse(IsChip(layout, TextOf(layout, "Research Lab ")), L"the line round the chip is plain");
    Assert::AreEqual(std::ptrdiff_t{3},
                     std::ranges::count_if(layout.texts, [&](const Outpost::Hud::Text& _text) { return IsChip(layout, _text); }));
    const Outpost::Hud::Text& enemyNodes = TextOf(layout, "2");
    Assert::IsTrue(enemyNodes.color.x == 1.0f && enemyNodes.color.y == 0.38f, L"the enemy's figure in the enemy's color");
    Assert::IsFalse(IsChip(layout, enemyNodes), L"and plain");
  }

  // ADR-086: the tag in the top-right corner says where the match runs, and warns once no snapshot has come for two
  // seconds, its edge standing still as the seconds count; without a connection there is no tag.
  TEST_METHOD(ShowsWhereTheMatchRuns)
  {
    // The tag's own panel: the widest that is not a chip's under _text's first pixel.
    const auto tagOf = [](const Outpost::Hud::Layout& _layout, const Outpost::Hud::Text& _text)
    {
      Outpost::Hud::Rect tag;
      for (const Outpost::Hud::Rect& panel : _layout.panels)
      {
        const bool chip = panel.color.x == 1.0f && panel.color.y == 0.5f && panel.color.z == 0.35f;
        if (!chip && panel.Contains(std::round(_text.left) + 0.5f, std::round(_text.top) + 0.5f) && panel.width > tag.width)
          tag = panel;
      }
      return tag;
    };
    Outpost::Hud::Content content{.ore = 650, .oreIncomeHundredthsPerSecond = 650};
    Assert::IsFalse(
      std::ranges::any_of(Lay(content, 1920, 1080).texts, [](const Outpost::Hud::Text& _text) { return _text.text.starts_with("Local"); }),
      L"no tag without a connection");

    content.connection = Outpost::Hud::Connection{};
    const Outpost::Hud::Layout local = Lay(content, 1920, 1080);
    const Outpost::Hud::Text& localText = TextOf(local, "Local skirmish");
    Assert::IsFalse(IsChip(local, localText));
    const Outpost::Hud::Rect localTag = tagOf(local, localText);
    Assert::AreEqual(1920.0f - 16.0f, localTag.left + localTag.width, L"anchored to the top-right corner");
    Assert::AreEqual(16.0f, localTag.top);

    content.connection->server = Outpost::Hud::ServerName("203.0.113.5", 4433);
    content.connection->silentSeconds = 1;
    const Outpost::Hud::Layout server = Lay(content, 1920, 1080);
    const Outpost::Hud::Rect serverTag = tagOf(server, TextOf(server, "Server 203.0.113.5:4433"));
    Assert::AreEqual(1920.0f - 16.0f, serverTag.left + serverTag.width);
    Assert::IsTrue(serverTag.width > localTag.width, L"as wide as its words");
    Assert::IsFalse(std::ranges::any_of(server.texts, [](const Outpost::Hud::Text& _text) { return _text.text.starts_with("No word"); }),
                    L"a second without a snapshot is no warning");

    // Silent: a chip under the address, the tag as wide at two seconds as at nine hundred.
    std::vector<float> widths;
    for (const std::int32_t seconds : {2, 10, 999, 5000})
    {
      content.connection->silentSeconds = seconds;
      const Outpost::Hud::Layout silent = Lay(content, 1920, 1080);
      const std::string words = std::format("No word from the server \xC2\xB7 {} s", std::min(seconds, 999));
      const Outpost::Hud::Text& warning = TextOf(silent, words);
      Assert::IsTrue(IsChip(silent, warning), std::wstring(winrt::to_hstring(words)).c_str());
      const Outpost::Hud::Text& address = TextOf(silent, "Server 203.0.113.5:4433");
      Assert::IsTrue(warning.top > address.top, L"under the address");
      const Outpost::Hud::Rect tag = tagOf(silent, address);
      Assert::AreEqual(1920.0f - 16.0f, tag.left + tag.width);
      Assert::IsTrue(tag.Contains(warning.left, warning.top), L"inside the tag");
      widths.push_back(tag.width);
    }
    Assert::IsTrue(std::ranges::all_of(widths, [&](float _width) { return _width == widths.front(); }), L"its edge stands still");

    // An address too long for the tag is cut short, and the tag stays in bounds.
    content.connection = Outpost::Hud::Connection{.server = Outpost::Hud::ServerName(std::string(80, 'h'), 4433)};
    const Outpost::Hud::Layout longest = Lay(content, 1920, 1080);
    const auto cut =
      std::ranges::find_if(longest.texts, [](const Outpost::Hud::Text& _text) { return _text.text.starts_with("Server h"); });
    Assert::IsTrue(cut != longest.texts.end());
    Assert::IsTrue(cut->text.size() < 80, L"cut short");
    Assert::IsTrue(tagOf(longest, *cut).width <= 360.0f + (2.0f * 12.0f));
  }

  // ADR-086: a server's address is its host and port, an IPv6 host in brackets so that its port reads apart.
  TEST_METHOD(WritesAServerName)
  {
    Assert::AreEqual(std::string("203.0.113.5:4433"), Outpost::Hud::ServerName("203.0.113.5", 4433));
    Assert::AreEqual(std::string("outpost.example:1"), Outpost::Hud::ServerName("outpost.example", 1));
    Assert::AreEqual(std::string("[2001:db8::5]:4433"), Outpost::Hud::ServerName("2001:db8::5", 4433));
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
    Assert::AreEqual(size_t{4}, content.buttons.size(), L"the three it builds, and its retreat");
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
    Assert::AreEqual(size_t{2}, Outpost::Hud::Describe(newest, entities, selected).buttons.size(), L"the Shipyard, and its retreat");

    newest.sectors = {{.id = 1, .nameUtf8 = "South", .maxXMeters = 100.0f, .maxZMeters = 100.0f, .holder = PLAYER}};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, entities, selected);
    Assert::AreEqual(size_t{3}, content.buttons.size());
    Assert::IsTrue(content.buttons[1].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::Build, .structure = Outpost::StructureKind::Relay});
  }

  // ADR-073: a sector the pirates guard is shown as theirs on the minimap, and counts for neither side; their ships and
  // structures are marked in their color.
  // ADR-074: a derelict under the pointer says what it holds, and a topic recovered by salvage says so on its card.
  TEST_METHOD(DescribesADerelict)
  {
    Outpost::Snapshot newest = Newest();
    newest.research = {{.id = Outpost::ResearchTopicId{5}, .nameUtf8 = "Fusion Drive", .recovered = true}};
    Outpost::EntityView derelict{.id = Outpost::EntityId{40}, .kind = Outpost::EntityKind::Derelict, .salvageOre = 1200};
    Assert::AreEqual(std::string("Derelict: 1,200 Ore"), Outpost::Hud::DescribeDerelict(newest, derelict, 20));
    derelict.salvageTopic = Outpost::ResearchTopicId{5};
    derelict.salvagePermille = 333;
    derelict.remembered = true;
    newest.tick = 2405;
    derelict.lastSeenTick = 5;
    Assert::AreEqual(
      std::string("Derelict: 1,200 Ore, and part of Fusion Drive's research time \xC2\xB7 salvaged 33% (last seen 2:00 ago)"),
      Outpost::Hud::DescribeDerelict(newest, derelict, 20));
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, std::vector<Outpost::EntityView>{derelict}, {});
    Assert::IsTrue(content.marks.front().kind == Outpost::EntityKind::Derelict &&
                   content.marks.front().side == Outpost::Hud::Side::Neutral);
  }

  // Interface plan 2, task UI1.2: a structure the player only remembers says how long ago it was last seen, under the
  // pointer and on its selection panel, where what follows is as it was then; with no tick rate its age is not told.
  TEST_METHOD(DescribesAMemoryWithItsAge)
  {
    Outpost::Snapshot newest = Newest();
    newest.tick = 20 * 252 + 7;
    newest.structureTypes = {{.structure = Outpost::StructureKind::Shipyard, .nameUtf8 = "Shipyard", .radiusMeters = 40.0f}};
    const Outpost::EntityView yard{.id = Outpost::EntityId{41},
                                   .kind = Outpost::EntityKind::Structure,
                                   .owner = Outpost::PlayerId{2},
                                   .structure = Outpost::StructureKind::Shipyard,
                                   .hitPointsHundredths = 120'000,
                                   .maxHitPointsHundredths = 300'000,
                                   .builtPermille = 340,
                                   .remembered = true,
                                   .lastSeenTick = 7};
    Assert::AreEqual(std::string("Shipyard \xC2\xB7 last seen 4:12 ago, 34% built"), Outpost::Hud::DescribeMemory(newest, yard, 20));

    const std::vector<Outpost::EntityView> entities{yard};
    const std::vector<Outpost::EntityId> selected{yard.id};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, entities, selected, std::nullopt, nullptr, std::nullopt, 20);
    Assert::AreEqual(std::string("Last seen 4:12 ago"), content.selection.at(1));
    Assert::AreEqual(std::string("34% built when seen"), content.selection.at(2));
    Assert::AreEqual(std::string("Hit points 1,200 / 3,000 when seen"), content.selection.at(3));
    Assert::IsTrue(content.marks.front().remembered);

    Assert::AreEqual(std::string("Last seen earlier"), Outpost::Hud::Describe(newest, entities, selected).selection.at(1), L"no tick rate");
    Outpost::EntityView seen = yard;
    seen.remembered = false;
    seen.lastSeenTick = 0;
    const std::vector<Outpost::EntityView> live{seen};
    const Outpost::Hud::Content now = Outpost::Hud::Describe(newest, live, selected, std::nullopt, nullptr, std::nullopt, 20);
    Assert::AreEqual(std::string("Under construction, 34%"), now.selection.at(1), L"one seen now tells no age");
    Assert::IsFalse(now.marks.front().remembered);
  }

  // Interface plan 2, task UI1.2: on the minimap a memory is a cross in its side's color, drawn over the fog so that the
  // fog does not dim it twice, where what is seen is a filled square under the fog.
  TEST_METHOD(DrawsAMemoryAsACrossOverTheFog)
  {
    Outpost::Hud::Content content{.ore = 0, .selection = {}, .mapSizeMeters = 2000.0f, .fog = true};
    const Outpost::PlanePosition memory{.xMeters = -500.0f};
    const Outpost::PlanePosition seen{.xMeters = 500.0f};
    content.marks = {{.position = memory,
                      .radiusMeters = 40.0f,
                      .side = Outpost::Hud::Side::Enemy,
                      .kind = Outpost::EntityKind::Structure,
                      .remembered = true},
                     {.position = seen, .radiusMeters = 40.0f, .side = Outpost::Hud::Side::Enemy, .kind = Outpost::EntityKind::Structure}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const auto fog =
      std::ranges::find_if(layout.panels, [](const Outpost::Hud::Rect& _panel) { return _panel.fill == Outpost::Hud::Fill::Fog; });
    Assert::IsTrue(fog != layout.panels.end());
    const auto marksAt = [&layout](Outpost::PlanePosition _position, float _dx, float _dy)
    {
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(_position);
      std::vector<std::ptrdiff_t> found;
      for (auto panel = layout.panels.begin(); panel != layout.panels.end(); ++panel)
      {
        if (panel->fill != Outpost::Hud::Fill::Fog && panel->width < 100.0f && panel->Contains(at.x + _dx, at.y + _dy))
          found.push_back(panel - layout.panels.begin());
      }
      return found;
    };
    const std::ptrdiff_t fogAt = fog - layout.panels.begin();
    const std::vector<std::ptrdiff_t> middle = marksAt(memory, 0.0f, 0.0f);
    Assert::AreEqual(size_t{2}, middle.size(), L"two arms cross at its middle");
    Assert::IsTrue(std::ranges::all_of(middle, [fogAt](std::ptrdiff_t _index) { return _index > fogAt; }), L"over the fog");
    Assert::IsTrue(marksAt(memory, 3.0f, 3.0f).empty(), L"a cross, not a square: its corners are open");
    const std::vector<std::ptrdiff_t> square = marksAt(seen, 3.0f, 3.0f);
    Assert::AreEqual(size_t{1}, square.size());
    Assert::IsTrue(square.front() < fogAt, L"what is seen stays under the fog");
  }

  // Interface plan 2, task UI1.4: over the fog the minimap draws every sector's border, at 2:1 against the map, and a
  // cut-off sector's outline in dashes. A node has no mark of its own; while the player places a Relay, each sector whose
  // node it could claim now is outlined in its color inside the border (ADR-081 decision 7).
  TEST_METHOD(DrawsTheTerritoryOnTheMinimap)
  {
    Outpost::Snapshot newest = Newest();
    newest.mapSizeMeters = 3000.0f;
    newest.structureTypes = {{.structure = Outpost::StructureKind::Relay, .nameUtf8 = "Relay", .radiusMeters = 30.0f, .buildable = true}};
    newest.sectors = {{.id = 1,
                       .minXMeters = -1500.0f,
                       .maxXMeters = 0.0f,
                       .minZMeters = -1500.0f,
                       .maxZMeters = 1500.0f,
                       .node = {.xMeters = -750.0f},
                       .adjacent = {2},
                       .holder = PLAYER,
                       .cutOff = true},
                      {.id = 2,
                       .minXMeters = 0.0f,
                       .maxXMeters = 1500.0f,
                       .minZMeters = -1500.0f,
                       .maxZMeters = 1500.0f,
                       .node = {.xMeters = 750.0f},
                       .adjacent = {1}}};
    Outpost::Hud::Content content = Outpost::Hud::Describe(newest, std::vector<Outpost::EntityView>{}, {});
    Assert::IsTrue(content.sectors[0].cutOff);
    Assert::IsTrue(std::ranges::none_of(content.sectors, &Outpost::Hud::SectorMark::claimable), L"none unless a Relay is placed");
    Outpost::Hud::Content placing = Outpost::Hud::Describe(newest, std::vector<Outpost::EntityView>{}, {}, Outpost::StructureKind::Relay);
    Assert::IsTrue(!placing.sectors[0].claimable && placing.sectors[1].claimable, L"while placing, the free one next to the player's");
    content.fog = true;
    placing.fog = true;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const Outpost::Hud::Layout placingLayout = Lay(placing, 1920, 1080);
    // A line, a dot or an outline's side at a point: not the map, a sector's wash or the fog.
    const auto thinAt = [](const Outpost::Hud::Layout& _layout, float _x, float _y)
    {
      return std::ranges::count_if(_layout.panels,
                                   [&](const Outpost::Hud::Rect& _panel)
                                   {
                                     const bool thin =
                                       std::min(_panel.width, _panel.height) <= 2.5f || (_panel.width <= 14.0f && _panel.height <= 14.0f);
                                     return _panel.fill == Outpost::Hud::Fill::Solid && thin && _panel.Contains(_x, _y);
                                   });
    };

    // The free sector's border along the map's top, a little in from its corner.
    const DirectX::XMFLOAT2 topRight = layout.MinimapPixelOf({.xMeters = 1400.0f, .zMeters = 1500.0f});
    const auto border = std::ranges::find_if(layout.panels, [&](const Outpost::Hud::Rect& _panel)
                                             { return _panel.Contains(topRight.x, topRight.y + 0.5f) && _panel.height <= 2.0f; });
    Assert::IsTrue(border != layout.panels.end(), L"a free sector's border");
    // The map under it, within a sector and clear of its lines and marks, without the fog.
    const DirectX::XMFLOAT2 inside = layout.MinimapPixelOf({.xMeters = 1000.0f, .zMeters = 1000.0f});
    const std::vector<DirectX::XMFLOAT3> grounds = GroundsOf(layout, 0, Outpost::Hud::Text{.text = "x", .left = inside.x, .top = inside.y});
    Assert::IsTrue(ContrastOf({border->color.x, border->color.y, border->color.z}, grounds.front()) >= 2.0f, L"at 2:1");

    // No mark on a node, placing a Relay or not.
    for (const Outpost::SectorView& sector : newest.sectors)
    {
      const DirectX::XMFLOAT2 node = layout.MinimapPixelOf(sector.node);
      Assert::AreEqual(std::ptrdiff_t{0}, thinAt(layout, node.x, node.y), L"no mark on the node");
      Assert::AreEqual(std::ptrdiff_t{0}, thinAt(placingLayout, node.x, node.y), L"none while placing");
    }
    // The free sector outlined in the player's color at full strength, clear of its border and so of the held sector's
    // outline beside it, only while a Relay is placed.
    const DirectX::XMFLOAT2 freeLow = layout.MinimapPixelOf({.xMeters = 0.0f, .zMeters = 1500.0f});
    const DirectX::XMFLOAT2 freeHigh = layout.MinimapPixelOf({.xMeters = 1500.0f, .zMeters = -1500.0f});
    const auto ownInside = [&](const Outpost::Hud::Layout& _layout)
    {
      return std::ranges::count_if(_layout.panels,
                                   [&](const Outpost::Hud::Rect& _panel)
                                   {
                                     return _panel.color.x == 0.35f && _panel.color.z == 1.0f && _panel.color.w == 1.0f &&
                                            _panel.left > freeLow.x + 1.0f && _panel.top > freeLow.y + 1.0f &&
                                            _panel.left + _panel.width < freeHigh.x - 1.0f &&
                                            _panel.top + _panel.height < freeHigh.y - 1.0f;
                                   });
    };
    Assert::AreEqual(std::ptrdiff_t{0}, ownInside(layout), L"no outline unless a Relay is placed");
    Assert::AreEqual(std::ptrdiff_t{4}, ownInside(placingLayout), L"the claimable sector's four sides");

    // The cut-off sector's left side, along the map's left edge: dashes, with gaps between them.
    const DirectX::XMFLOAT2 low = layout.MinimapPixelOf({.xMeters = -1500.0f, .zMeters = -1500.0f});
    const DirectX::XMFLOAT2 high = layout.MinimapPixelOf({.xMeters = -1500.0f, .zMeters = 1500.0f});
    int covered = 0;
    int open = 0;
    for (int pixel = 1; high.y + static_cast<float>(pixel) < low.y - 1.0f; ++pixel)
    {
      const float y = high.y + static_cast<float>(pixel);
      const bool outlined = std::ranges::any_of(
        layout.panels, [&](const Outpost::Hud::Rect& _panel)
        { return _panel.color.x == 0.35f && _panel.color.z == 1.0f && _panel.width <= 2.5f && _panel.Contains(low.x + 0.5f, y); });
      (outlined ? covered : open) += 1;
    }
    Assert::IsTrue(covered > 0 && open > 0, L"dashed, not whole");
  }

  TEST_METHOD(ShowsThePirates)
  {
    Outpost::Snapshot newest = Newest();
    newest.mapSizeMeters = 3000.0f;
    newest.sectors = {
      {.id = 1, .minXMeters = -1500.0f, .maxXMeters = 0.0f, .minZMeters = -1500.0f, .maxZMeters = 1500.0f, .holder = PLAYER},
      {.id = 2, .minXMeters = 0.0f, .maxXMeters = 1500.0f, .minZMeters = -1500.0f, .maxZMeters = 1500.0f, .guarded = true}};
    const std::vector<Outpost::EntityView> entities{
      {.id = Outpost::EntityId{40}, .kind = Outpost::EntityKind::Ship, .owner = Outpost::PIRATES, .position = {.xMeters = 700.0f}}};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, entities, {});
    Assert::IsTrue(content.sectors[1].side == Outpost::Hud::Side::Pirate);
    Assert::AreEqual(0, content.territory.value_or(Outpost::Hud::Territory{}).enemyNodes);
    Assert::IsTrue(content.marks.front().side == Outpost::Hud::Side::Pirate);
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
    newest.nodeCap = 3;
    Outpost::Hud::Content content = Outpost::Hud::Describe(newest, {}, {});
    Assert::IsTrue(content.territory.has_value());
    const Outpost::Hud::Territory territory = content.territory.value_or(Outpost::Hud::Territory{});
    Assert::AreEqual(1, territory.ownNodes);
    Assert::AreEqual(1, territory.enemyNodes);
    Assert::AreEqual(3, territory.nodes);
    Assert::AreEqual(3, territory.cap);
    Assert::AreEqual(size_t{3}, content.sectors.size());
    Assert::IsTrue(content.sectors[0].side == Outpost::Hud::Side::Own && content.sectors[2].side == Outpost::Hud::Side::Enemy);

    content.fog = true;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    // The nodes as the tickets read, the player's against the enemy's, with the station's cap of the map's beside "Nodes" in
    // the labels' color (interface plan 2, task UI3.3).
    const Outpost::Hud::Text& nodes = TextOf(layout, "Nodes");
    const Outpost::Hud::Text& cap = TextOf(layout, "cap 3 of 3");
    Assert::IsTrue(cap.top == nodes.top && cap.left > nodes.left, L"the cap beside the label");
    Assert::IsTrue(cap.color.x == 0.22f && cap.color.z == 0.42f, L"in the labels' color");
    Assert::IsTrue(TextOf(layout, "1 : ").left < TextOf(layout, "1").left, L"the player's, then the enemy's");
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
  // ADR-066: a line says what its front job is and how far it has come. Task 15.2: each window's button shows its key.
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
    const std::vector<std::string> expected{"Shipyard", "Hit points 2,500 / 2,500",
                                            "Building Small+Ion+Mass Driver \xC2\xB7 45% \xC2\xB7 +1 queued"};
    Assert::IsTrue(content.selection == expected, L"the rest of the queue is the production window's");
    Assert::AreEqual(size_t{2}, content.buttons.size());
    Assert::IsTrue(content.buttons[0].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::OpenProduction, .producer = yard.id});
    Assert::IsTrue(content.buttons[1].action == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::OpenDesigner, .producer = yard.id});
    Assert::AreEqual(std::string("P"), content.buttons[0].key);
    Assert::AreEqual(std::string("D"), content.buttons[1].key);

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
    Assert::AreEqual(std::string("R"), content.buttons[0].key);

    Outpost::EntityView theirs = yard;
    theirs.owner = Outpost::PlayerId{2};
    Assert::IsTrue(describe(theirs).buttons.empty(), L"not the enemy's");

    // Under construction, it says how far it has come, and opens nothing.
    yard.builtPermille = 600;
    content = describe(yard);
    Assert::AreEqual(std::string("Under construction, 60%"), content.selection[1]);
    Assert::IsTrue(content.buttons.empty());
  }

  // Phase 3 design §9: a structure that grows names its level, says what the next one gives, and offers it with its time
  // and cost, dim while a level is being built or the Ore is short; at the top there is no button.
  TEST_METHOD(NamesALevelAndOffersTheNext)
  {
    Outpost::Snapshot newest = Newest();
    newest.ore = 200;
    newest.structureTypes = {{.structure = Outpost::StructureKind::Shipyard,
                              .nameUtf8 = "Shipyard",
                              .buildable = true,
                              .cost = 300,
                              .levels = {{.cost = 150, .buildSeconds = 30.0, .maxHitPointsHundredths = 300000},
                                         {.cost = 300, .buildSeconds = 60.0, .maxHitPointsHundredths = 350000}}},
                             {.structure = Outpost::StructureKind::DefensePlatform, .nameUtf8 = "Defence Platform", .buildable = true}};
    Outpost::EntityView yard{.id = Outpost::EntityId{30},
                             .kind = Outpost::EntityKind::Structure,
                             .owner = PLAYER,
                             .structure = Outpost::StructureKind::Shipyard,
                             .hitPointsHundredths = 250000,
                             .maxHitPointsHundredths = 250000,
                             .shipyardNumber = 1};
    const std::vector<Outpost::EntityId> selected{yard.id};
    const auto describe = [&](const Outpost::EntityView& _entity)
    { return Outpost::Hud::Describe(newest, std::vector{_entity}, selected); };
    const Outpost::Hud::Action upgrade{.kind = Outpost::Hud::ActionKind::Upgrade, .producer = yard.id};

    Outpost::Hud::Content content = describe(yard);
    std::vector<std::string> expected{"Shipyard 01 \xC2\xB7 L1", "Hit points 2,500 / 2,500", "Idle", "L2: 3,000 hit points"};
    Assert::IsTrue(content.selection == expected);
    Assert::AreEqual(size_t{3}, content.buttons.size());
    Assert::AreEqual(std::string("Upgrade to L2 \xC2\xB7 0:30|150"), content.buttons[2].label);
    Assert::IsTrue(content.buttons[2].action == upgrade && content.buttons[2].enabled);
    Assert::IsFalse(content.buttons[2].progressPermille.has_value());

    // Building the level: how far it has come, and the button dim with the reason and a bar for the work.
    yard.upgradePermille = 417;
    content = describe(yard);
    expected = {"Shipyard 01 \xC2\xB7 L1", "Upgrading to L2, 41%", "Hit points 2,500 / 2,500", "Idle", "L2: 3,000 hit points"};
    Assert::IsTrue(content.selection == expected);
    Assert::IsFalse(content.buttons[2].enabled);
    Assert::AreEqual(std::string("UPGRADING \xC2\xB7 41%"), content.buttons[2].note);
    Assert::IsTrue(content.buttons[2].progressPermille == 417);

    // Short of Ore: dim, and the cost says why.
    yard.upgradePermille.reset();
    yard.level = 2;
    content = describe(yard);
    Assert::AreEqual(std::string("Upgrade to L3 \xC2\xB7 1:00|300"), content.buttons[2].label);
    Assert::IsFalse(content.buttons[2].enabled);
    Assert::IsTrue(content.buttons[2].note.empty());
    Assert::IsFalse(content.buttons[2].progressPermille.has_value());

    yard.level = 3;
    content = describe(yard);
    Assert::AreEqual(std::string("Top level"), content.selection.back());
    Assert::AreEqual(size_t{2}, content.buttons.size(), L"no level left to buy");

    // The enemy's names its level and offers nothing; a kind that does not grow names none.
    Outpost::EntityView theirs = yard;
    theirs.owner = Outpost::PlayerId{2};
    theirs.shipyardNumber = 0;
    content = describe(theirs);
    Assert::AreEqual(std::string("Shipyard \xC2\xB7 L3"), content.selection.front());
    Assert::IsTrue(content.buttons.empty());
    Outpost::EntityView platform = yard;
    platform.structure = Outpost::StructureKind::DefensePlatform;
    platform.shipyardNumber = 0;
    platform.level = 1;
    Assert::AreEqual(std::string("Defence Platform"), describe(platform).selection.front());
  }

  // The work a button started runs as a bar along its foot, from its left edge, as far as the work has come.
  TEST_METHOD(DrawsAButtonsWorkAsABar)
  {
    Outpost::Hud::Content content{.ore = 0,
                                  .buttons = {{.label = "Upgrade to L2|150", .enabled = false, .note = "UPGRADING \xC2\xB7 25%"}}};
    const std::size_t idle = Lay(content, 1920, 1080).panels.size();
    content.buttons[0].progressPermille = 250;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(idle + 2, layout.panels.size(), L"a track and its fill");
    const auto track = std::ranges::adjacent_find(
      layout.panels, [](const Outpost::Hud::Rect& _track, const Outpost::Hud::Rect& _fill)
      { return _fill.left == _track.left && _fill.top == _track.top && _fill.height == _track.height && _fill.width < _track.width; });
    Assert::IsTrue(track != layout.panels.end());
    Assert::AreEqual(track->width / 4.0f, std::next(track)->width, 0.01f);
  }

  // Phase 3 design §7: a Command Station's next level names the nodes it lets the player hold and the Defence guns it adds;
  // at the cap, a Constructor's Relay is dim with the reason, and so is placing one.
  TEST_METHOD(ShowsTheStationsCap)
  {
    Outpost::Snapshot newest = Newest();
    newest.ore = 1000;
    newest.structureTypes = {{.structure = Outpost::StructureKind::CommandStation,
                              .nameUtf8 = "Command Station",
                              .levels = {{.cost = 300, .buildSeconds = 45.0, .maxHitPointsHundredths = 600000, .nodes = 4},
                                         {.cost = 500, .buildSeconds = 60.0, .maxHitPointsHundredths = 700000, .nodes = 5, .guns = 2}}},
                             {.structure = Outpost::StructureKind::Relay, .nameUtf8 = "Relay", .buildable = true, .cost = 200}};
    Outpost::EntityView station{.id = Outpost::EntityId{20},
                                .kind = Outpost::EntityKind::Structure,
                                .owner = PLAYER,
                                .structure = Outpost::StructureKind::CommandStation,
                                .hitPointsHundredths = 500000,
                                .maxHitPointsHundredths = 500000};
    const auto levelLine = [&](const std::string& _line)
    {
      const std::vector<std::string> selection = Outpost::Hud::Describe(newest, std::vector{station}, std::vector{station.id}).selection;
      return std::ranges::find(selection, _line) != selection.end();
    };
    Assert::IsTrue(levelLine("L2: 4 nodes, 6,000 hit points"));
    station.level = 2;
    Assert::IsTrue(levelLine("L3: 5 nodes, 2 Defence guns, 7,000 hit points"));

    // The player holds its home and has a cap of one: the Relay waits for the station's next level.
    newest.sectors = {{.id = 1, .nameUtf8 = "South", .maxXMeters = 100.0f, .maxZMeters = 100.0f, .holder = PLAYER}};
    newest.nodeCap = 1;
    Outpost::EntityView constructor = Ship(9, {}, 30000, 30000);
    constructor.role = Outpost::ShipRole::Constructor;
    const std::vector<Outpost::EntityView> entities{constructor};
    const std::vector<Outpost::EntityId> selected{constructor.id};
    Outpost::Hud::Content content = Outpost::Hud::Describe(newest, entities, selected);
    Assert::AreEqual(size_t{2}, content.buttons.size(), L"the Relay, and its retreat");
    Assert::IsFalse(content.buttons[0].enabled);
    Assert::AreEqual(std::string("NODE CAP"), content.buttons[0].note);
    content = Outpost::Hud::Describe(newest, entities, selected, Outpost::StructureKind::Relay);
    Assert::AreEqual(std::string("Relay: at the node cap; upgrade the Command Station to claim more"), content.hint);

    newest.nodeCap = 2;
    content = Outpost::Hud::Describe(newest, entities, selected);
    Assert::IsTrue(content.buttons[0].enabled && content.buttons[0].note.empty(), L"below the cap");
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
    // A mark's color: an ore asteroid's mark is an outline (ADR-068), so a panel of it near the point, not over it.
    const auto colorAt = [&layout](Outpost::PlanePosition _position)
    {
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(_position);
      const auto panel = std::ranges::find_if(layout.panels,
                                              [&at](const Outpost::Hud::Rect& _rect)
                                              {
                                                return std::abs(_rect.left + (_rect.width / 2.0f) - at.x) < 10.0f &&
                                                       std::abs(_rect.top + (_rect.height / 2.0f) - at.y) < 10.0f && _rect.width < 40.0f;
                                              });
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
    Assert::AreEqual(300.0f - 16.0f, map.width, 0.01f, L"300 units square, less its padding (ADR-068)");
    Assert::AreEqual(1080.0f - 16.0f - 300.0f + 8.0f, map.top, 0.01f);

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
  // under the Ore, in the status panel (ADR-066).
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
    Assert::AreEqual(size_t{1}, unselected.status.size());
    Assert::AreEqual(std::string("Hull Plating"), unselected.status[0].Text());
    Assert::AreEqual(0.25f, unselected.status[0].bar.value_or(Outpost::Hud::StatusBar{}).share, 1e-6f);
    const Outpost::Hud::Layout layout = Lay(unselected, 1920, 1080);
    Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "Hull Plating"; }));
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
    // Phase 4 design §10: the design's retreat, which a press steps; a saved design's new retreat is an update.
    Assert::AreEqual(std::string("RETREAT AT 25%"), panel.retreat.label);
    Assert::IsTrue(panel.retreat.action.kind == Outpost::Hud::ActionKind::StepRetreat);
    {
      Outpost::Designer stepped = designer;
      stepped.StepRetreat(newest);
      const Outpost::Hud::DesignerPanel changed = DesignerOf(newest, stepped);
      Assert::AreEqual(std::string("RETREAT AT 50%"), changed.retreat.label);
      Assert::IsTrue(changed.rename.enabled);
      Assert::AreEqual(std::string("UPDATE"), changed.rename.label);
    }

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

    // Against a formation of each hull, per ship and per 100 Ore, and rated against the best any design does to it: the
    // Missile Rack's 14 a ship to the five Small hulls a hit reaches, 70 in all (ADR-014), and the Lance's 32.2 to the Medium
    // and 30.0 to the Large, whose formations a splash does not reach past the target.
    Assert::AreEqual(size_t{3}, panel.damage.size());
    Assert::AreEqual(std::string("30.0"), panel.damage[0].perShip);
    Assert::AreEqual(std::string("34.5 / 100 ore"), panel.damage[0].perOre);
    Assert::IsTrue(panel.damage[0].rating == Outpost::Hud::Rating::Fair);
    Assert::IsTrue(panel.damage[0].reach.empty(), L"no splash");
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

  // ADR-014: a splash weapon's card counts every ship of a formation one hit reaches, and says how many. The Missile Rack's
  // 30 m reaches five Small hulls standing 24 m apart, and no Medium or Large neighbor.
  TEST_METHOD(CountsASplashAgainstAFormation)
  {
    const Outpost::Snapshot newest = DesignerSnapshot(true);
    Outpost::Designer designer;
    designer.Update(newest);
    designer.PickHull(Outpost::HullId{1});
    designer.PickDrive(Outpost::DriveId{1});
    designer.PickWeapon(Outpost::WeaponId{3});
    const Outpost::Hud::DesignerPanel panel = DesignerOf(newest, designer);
    Assert::AreEqual(std::string("70.0"), panel.damage[0].perShip, L"14 a ship, five ships");
    Assert::AreEqual(std::string("\xC3\x97"
                                 "5 ships"),
                     panel.damage[0].reach);
    Assert::AreEqual(std::string("38.5 / 100 ore"), panel.damage[0].perOre);
    Assert::IsTrue(panel.damage[0].rating == Outpost::Hud::Rating::Good);
    Assert::AreEqual(std::string("11.0"), panel.damage[1].perShip);
    Assert::IsTrue(panel.damage[1].reach.empty() && panel.damage[2].reach.empty());
  }

  // Phase 1 design §11: hovering a part previews the design it would make, and how each number changes; lower is better
  // for cost and build time. Hovering the pick previews nothing. Task 15.3: each figure reads the previewed number and its
  // change as a signed number, a number that keeps reads alone, and a damage card adds its change per ship.
  TEST_METHOD(PreviewsAHoveredPart)
  {
    const Outpost::Snapshot newest = DesignerSnapshot();
    Outpost::Designer designer;
    designer.Update(newest);
    const Outpost::Hud::Action medium{.kind = Outpost::Hud::ActionKind::PickHull, .hull = Outpost::HullId{2}};
    const Outpost::Hud::DesignerPanel panel = DesignerOf(newest, designer, medium);
    Assert::AreEqual(std::string("Preview: with Medium instead"), panel.hint);
    using Change = Outpost::Hud::Change;
    // Hit points, armor and cost rise, speed falls, range keeps, and build time rises: cost and time are worse for it.
    const std::array<std::string, 6> values{"450 (+252)", "8 (+6)", "52 (-26)", "120", "165 (+78)", "16 (+8)"};
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
    Assert::IsTrue(panel.damage[0].perShipChange.empty(), L"no change to show");

    // The Lance in place of the Mass Driver: more damage to every hull, and each card says how much more, to a tenth, as
    // the figures are written: 30.0 to 34.4, 15.0 to 32.2, and 8.8 to 30.0.
    const Outpost::Hud::DesignerPanel lance =
      DesignerOf(newest, designer, Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::PickWeapon, .weapon = Outpost::WeaponId{2}});
    const std::array<std::string, 3> perShipChanges{"+4.4", "+17.2", "+21.2"};
    for (size_t i = 0; i < lance.damage.size(); ++i)
    {
      Assert::IsTrue(lance.damage[i].change == Change::Better);
      Assert::AreEqual(perShipChanges[i], lance.damage[i].perShipChange);
    }
    Assert::AreEqual(std::string("220 (+100)"), lance.bars[3].previewValue, L"range");

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

  // Phase 3 design §5, §9: a Shipyard below a hull's level cannot build it. The designer still saves the design, and its
  // Queue is dim with the level the hull needs; the production window dims the design and says so; and the Shipyard's
  // panel names the hulls its next level adds.
  TEST_METHOD(SaysWhatAShipyardCannotBuild)
  {
    Outpost::Snapshot newest = DesignerSnapshot(true);
    for (Outpost::HullView& hull : newest.hulls)
      hull.shipyardLevel = static_cast<std::int32_t>(hull.id.value);
    Outpost::Designer designer;
    designer.Update(newest);
    designer.PickHull(Outpost::HullId{3});
    Outpost::Hud::DesignerPanel panel = DesignerOf(newest, designer);
    Assert::IsFalse(panel.queue.enabled, L"Shipyard 01 is at level 1");
    Assert::AreEqual(std::string("Needs Shipyard L3"), panel.queueDetail);
    Assert::IsTrue(panel.save.enabled, L"it still designs and saves");
    newest.entities.front().level = 3;
    panel = DesignerOf(newest, designer);
    Assert::IsTrue(panel.queue.enabled);
    Assert::AreNotEqual(std::string("Needs Shipyard L3"), panel.queueDetail);

    // The production window: a Small design builds at level 1, a Medium one waits for level 2.
    newest.entities.front().level = 1;
    newest.designs = {{.id = SWARM, .nameUtf8 = "Small+Ion+Mass Driver", .hull = Outpost::HullId{1}, .cost = 87},
                      {.id = LINE, .nameUtf8 = "Medium+Ion+Lance", .hull = Outpost::HullId{2}, .cost = 215}};
    const Outpost::Hud::ProductionPanel production = Outpost::Hud::DescribeProduction(newest, &newest.entities.front());
    Assert::IsTrue(production.options[0].enabled);
    Assert::IsFalse(production.options[1].enabled);
    Assert::IsTrue(production.options[1].detail.ends_with("NEEDS L2"), L"says the level it needs");

    // The panel names the next level's hulls.
    newest.structureTypes = {{.structure = Outpost::StructureKind::Shipyard,
                              .nameUtf8 = "Shipyard",
                              .buildable = true,
                              .cost = 300,
                              .levels = {{.cost = 150, .buildSeconds = 30.0, .maxHitPointsHundredths = 300000},
                                         {.cost = 300, .buildSeconds = 60.0, .maxHitPointsHundredths = 350000}}}};
    const std::vector<Outpost::EntityId> selected{newest.entities.front().id};
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, newest.entities, selected);
    Assert::IsTrue(std::ranges::find(content.selection, std::string("L2: Medium hulls, 3,000 hit points")) != content.selection.end());
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

  // Phase 1 design §11, after the mockup at ADR-062's sizes: a window 818 units wide; the Shipyard's arrows in its title bar,
  // pressed rather than grabbed; a card for each unlocked part to click and none for a locked one, which is hatched; the
  // weapons wrap to a second line of cards once there are more than three; and Queue at the bottom.
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
    Assert::AreEqual(std::string("700 (+700)"), previewed->previewValue, L"from no sight of its own (task 15.3)");

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
    Assert::AreEqual(818.0f, window.frame.width, 0.01f);
    Assert::AreEqual(1920.0f - 16.0f - 818.0f, window.frame.left, 0.01f, L"top right");
    Assert::AreEqual(size_t{2}, count(mockup, Outpost::Hud::ActionKind::PickHull), L"the Large hull is locked");
    Assert::AreEqual(size_t{1}, count(mockup, Outpost::Hud::ActionKind::PickDrive));
    Assert::AreEqual(size_t{2}, count(mockup, Outpost::Hud::ActionKind::PickWeapon));
    Assert::AreEqual(size_t{1}, count(mockup, Outpost::Hud::ActionKind::LoadDesign));
    Assert::AreEqual(size_t{0}, count(mockup, Outpost::Hud::ActionKind::NextDesigns), L"one chip fits");
    Assert::IsTrue(std::ranges::any_of(mockup.panels, [](const Outpost::Hud::Rect& _panel)
                                       { return _panel.fill == Outpost::Hud::Fill::Hatched && _panel.width < 250.0f; }),
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

  // Phase 1 design §6: the research window lists the topics tier by tier, each with its tier. A topic of a tier the
  // Research Lab has not opened is dim, and names the level that opens it (Phase 3 design §6).
  TEST_METHOD(ListsTheTopicsTierByTier)
  {
    Outpost::Snapshot newest = Newest();
    newest.ore = 1000;
    newest.structureTypes = {{.structure = Outpost::StructureKind::ResearchLab,
                              .nameUtf8 = "Research Lab",
                              .buildable = true,
                              .cost = 200,
                              .levels = {{.cost = 400, .buildSeconds = 60.0, .opensTier = 2}}}};
    newest.research = {{.id = Outpost::ResearchTopicId{10},
                        .nameUtf8 = "Pulse Drive",
                        .effectUtf8 = "Unlocks the Pulse drive",
                        .cost = 300,
                        .researchSeconds = 100.0,
                        .tier = 2},
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
                        .prerequisites = {Outpost::ResearchTopicId{2}},
                        .tier = 2}};
    const Outpost::EntityView lab{.id = Outpost::EntityId{40},
                                  .kind = Outpost::EntityKind::Structure,
                                  .owner = PLAYER,
                                  .structure = Outpost::StructureKind::ResearchLab};
    Outpost::Hud::ResearchPanel panel = Outpost::Hud::DescribeResearch(newest, std::vector{lab});
    Assert::AreEqual(size_t{3}, panel.topics.size());
    Assert::AreEqual(std::string("Hull Plating"), panel.topics[0].name, L"tier 1 first");
    Assert::AreEqual(std::string("TIER 1 \xC2\xB7 60 s"), panel.topics[0].time);
    Assert::IsTrue(panel.topics[0].enabled);
    Assert::AreEqual(2, panel.topics[1].tier);
    Assert::AreEqual(std::string("TIER 2 \xC2\xB7 100 s"), panel.topics[1].time);
    Assert::IsTrue(panel.topics[1].needs == std::vector<std::string>{"RESEARCH LAB L2"}, L"its tier waits for the Lab's level");
    Assert::IsFalse(panel.topics[1].enabled);
    Assert::IsTrue(panel.topics[2].needs == (std::vector<std::string>{"RESEARCH LAB L2", "HULL PLATING"}));

    // Once the Lab has opened tier 2, only the prerequisite is left.
    newest.researchTier = 2;
    panel = Outpost::Hud::DescribeResearch(newest, std::vector{lab});
    Assert::IsTrue(panel.topics[1].needs.empty() && panel.topics[1].enabled);
    Assert::IsTrue(panel.topics[2].needs == std::vector<std::string>{"HULL PLATING"});
  }

  // Phase 3 design §6: a level 4 Research Lab shows how far each of its two running topics has come, and a Lab's panel
  // names the tier its next level opens and the research it needs, its button dim until that is done.
  TEST_METHOD(ShowsTheLabsLevels)
  {
    Outpost::Snapshot newest = Newest();
    newest.ore = 1000;
    newest.research = {{.id = Outpost::ResearchTopicId{1}, .nameUtf8 = "Improved Extraction", .researched = false},
                       {.id = Outpost::ResearchTopicId{2}, .nameUtf8 = "Hull Plating", .researched = true},
                       {.id = Outpost::ResearchTopicId{3}, .nameUtf8 = "Mass Driver Calibration"}};
    newest.structureTypes = {{.structure = Outpost::StructureKind::ResearchLab,
                              .nameUtf8 = "Research Lab",
                              .buildable = true,
                              .cost = 200,
                              .levels = {{.cost = 400,
                                          .buildSeconds = 60.0,
                                          .maxHitPointsHundredths = 180000,
                                          .opensTier = 2,
                                          .prerequisites = {Outpost::ResearchTopicId{1}, Outpost::ResearchTopicId{2}}},
                                         {.cost = 600, .buildSeconds = 90.0, .maxHitPointsHundredths = 210000, .opensTier = 3},
                                         {.cost = 800, .buildSeconds = 120.0, .maxHitPointsHundredths = 240000, .researchSlots = 2}}}};
    Outpost::EntityView lab{.id = Outpost::EntityId{40},
                            .kind = Outpost::EntityKind::Structure,
                            .owner = PLAYER,
                            .structure = Outpost::StructureKind::ResearchLab,
                            .hitPointsHundredths = 150000,
                            .maxHitPointsHundredths = 150000};
    const std::vector<Outpost::EntityId> selected{lab.id};
    Outpost::Hud::Content content = Outpost::Hud::Describe(newest, std::vector{lab}, selected);
    Assert::IsTrue(std::ranges::find(content.selection, std::string("L2: tier 2, 1,800 hit points")) != content.selection.end());
    Assert::IsTrue(std::ranges::find(content.selection, std::string("L2 needs Improved Extraction")) != content.selection.end());
    Assert::IsFalse(content.buttons.back().enabled);
    Assert::AreEqual(std::string("NEEDS RESEARCH"), content.buttons.back().note);

    newest.research[0].researched = true;
    content = Outpost::Hud::Describe(newest, std::vector{lab}, selected);
    Assert::IsTrue(content.buttons.back().enabled);

    lab.level = 3;
    content = Outpost::Hud::Describe(newest, std::vector{lab}, selected);
    Assert::IsTrue(std::ranges::find(content.selection, std::string("L4: a second research slot, 2,400 hit points")) !=
                   content.selection.end());

    // Two topics running at once.
    lab.level = 4;
    lab.research = {Outpost::ResearchTopicId{3}, Outpost::ResearchTopicId{1}};
    lab.jobPermille = 300;
    lab.secondJobPermille = 120;
    const Outpost::Hud::ResearchPanel panel = Outpost::Hud::DescribeResearch(newest, std::vector{lab});
    Assert::AreEqual(size_t{2}, panel.queue.size());
    Assert::IsTrue(panel.queue[0].front && panel.queue[0].permille == 300);
    Assert::IsTrue(panel.queue[1].front && panel.queue[1].permille == 120 && !panel.queue[1].waiting);
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

    // With no fog and no view, the marks are the last panels, in order: the ore asteroid's outline, its top first, and the
    // field's square (ADR-068).
    Assert::IsTrue(layout.panels.size() >= 5);
    const Outpost::Hud::Rect& ore = layout.panels[layout.panels.size() - 5];
    const Outpost::Hud::Rect& field = layout.panels.back();
    Assert::IsTrue(field.width > ore.width, L"the field's square is the larger");
    const auto luminance = [](const DirectX::XMFLOAT4& _color)
    { return (0.2126f * _color.x) + (0.7152f * _color.y) + (0.0722f * _color.z); };
    Assert::IsTrue(luminance(field.color) < luminance(ore.color));
    Assert::IsTrue(ore.color.x > ore.color.y && ore.color.y > ore.color.z, L"gold");
  }

  // ADR-046: the Ore's gem and figure start at the panel's left whatever the figure, and the income keeps to its right.
  TEST_METHOD(KeepsTheOreGemStillAsTheFigureChanges)
  {
    const auto oreAt = [](std::int32_t _ore)
    {
      const Outpost::Hud::Layout layout = Lay({.ore = _ore, .oreIncomeHundredthsPerSecond = 2700}, 1920, 1080);
      const auto gem = std::ranges::find(layout.sprites, Outpost::Hud::Sprite::OreMark, &Outpost::Hud::SpriteMark::sprite);
      const auto figure = std::ranges::find(layout.texts, Outpost::WithThousands(_ore), &Outpost::Hud::Text::text);
      const auto income = std::ranges::find(layout.texts, std::string("+27/s"), &Outpost::Hud::Text::text);
      Assert::IsTrue(gem != layout.sprites.end() && figure != layout.texts.end() && income != layout.texts.end());
      return std::array<float, 3>{gem->area.left, figure->left, income->left};
    };
    const std::array<float, 3> oneDigit = oreAt(5);
    const std::array<float, 3> eightDigits = oreAt(12345678);
    Assert::AreEqual(oneDigit[0], eightDigits[0], 0.01f, L"the gem");
    Assert::AreEqual(oneDigit[1], eightDigits[1], 0.01f, L"the figure");
    Assert::AreEqual(oneDigit[2], eightDigits[2], 0.01f, L"the income");
    Assert::IsTrue(oneDigit[0] < 40.0f, L"at the panel's left");
  }

  // ADR-046: the selection's hit points show as a bar under its lines, as long as the share left, red when low.
  TEST_METHOD(DrawsTheSelectionsHealthAsABar)
  {
    const Outpost::Hud::Content healthy{
      .ore = 0, .oreIncomeHundredthsPerSecond = 650, .selection = {"Shipyard", "Hit points 3,000 / 3,000"}};
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
    // The ore asteroid's outline, its top first, then the rig's square (ADR-068).
    const Outpost::Hud::Rect& ore = layout.panels[layout.panels.size() - 5];
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
                                        .status = {{.runs = {{.text = "Researching Hull Plating, 25%"}}}},
                                        .mapSizeMeters = 2000.0f};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const Outpost::Hud::Span sprites = layout.SpritesOf(0);
    const auto corners = std::count_if(layout.sprites.begin() + static_cast<std::ptrdiff_t>(sprites.first),
                                       layout.sprites.begin() + static_cast<std::ptrdiff_t>(sprites.end),
                                       [](const Outpost::Hud::SpriteMark& _mark) { return _mark.sprite == Outpost::Hud::Sprite::Corner; });
    Assert::AreEqual(std::ptrdiff_t{20}, corners, L"four for each of the Ore, the status, the selection, the buttons and the minimap");
  }

  // ADR-043: Ore is written one way, as Ore's gem and the figure grouped in thousands: the stockpile, a button's cost,
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
                     L"a gem for the stockpile, the button, the window's Ore and the card");
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
      const Outpost::Hud::Layout layout = Lay({.ore = 0, .oreIncomeHundredthsPerSecond = 650, .selection = std::move(_lines)}, 1920, 1080);
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

    // ADR-083: a battle matchup's end says how.
    newest.ending = Outpost::MatchEnding::FleetDestroyed;
    Assert::AreEqual(std::string("A fleet destroyed. Match length 6:13"), outcome().detail);
    newest.ending = Outpost::MatchEnding::TimeLimit;
    Assert::AreEqual(std::string("Out of time. Match length 6:13"), outcome().detail);
  }

  // Phase 5 design §8: in a world no match ends, and a player who has lost is told when its seat restarts at its start, or
  // that it waits for its start to be clear; the banner keeps the way back to the menu.
  TEST_METHOD(SaysWhenAFallenSeatRestarts)
  {
    Outpost::Snapshot newest = Newest();
    newest.tick = 1000;
    newest.restartTick = 1000 + (20 * 3725);
    const Outpost::Hud::Outcome fallen = Outpost::Hud::DescribeOutcome(newest, 20).value_or(Outpost::Hud::Outcome{});
    Assert::AreEqual(std::string("Empire fallen"), fallen.title);
    Assert::AreEqual(std::string("It restarts at your start in 1:02:05"), fallen.detail);
    newest.restartTick = 990;
    Assert::AreEqual(std::string("It restarts once your start is clear of the enemy"),
                     Outpost::Hud::DescribeOutcome(newest, 20).value_or(Outpost::Hud::Outcome{}).detail);

    Outpost::Hud::Content content;
    content.outcome = Outpost::Hud::DescribeOutcome(newest, 20);
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::IsTrue(std::ranges::find(layout.texts, std::string("Empire fallen"), &Outpost::Hud::Text::text) != layout.texts.end());
    Assert::IsTrue(
      std::ranges::any_of(layout.actions, [](const auto& _action) { return _action.second.kind == Outpost::Hud::ActionKind::BackToMenu; }));
  }

  // Phase 4 design §10, §13: a selection's retreat is the last button, which steps it for every ship from the first one's,
  // and a ship going back to be repaired says so.
  TEST_METHOD(SetsTheSelectionsRetreat)
  {
    Outpost::EntityView first = Ship(9, SWARM, 30000, 30000);
    first.retreat = Outpost::RetreatThreshold::Half;
    Outpost::EntityView second = Ship(10, SWARM, 3000, 30000);
    second.retreat = Outpost::RetreatThreshold::Half;
    second.retreating = true;
    std::vector<Outpost::EntityView> entities{first, second};
    const std::vector<Outpost::EntityId> selected{Outpost::EntityId{9}, Outpost::EntityId{10}};
    Outpost::Hud::Content content = Outpost::Hud::Describe(Newest(), entities, selected);
    Assert::AreEqual(std::string("Retreat at 50%"), content.buttons.back().label);
    Assert::IsTrue(content.buttons.back().action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::SetRetreat, .retreat = Outpost::RetreatThreshold::Never});
    Assert::AreEqual(std::string("1 retreating to be repaired"), content.selection.back());

    entities[1].retreat = Outpost::RetreatThreshold::Quarter;
    content = Outpost::Hud::Describe(Newest(), entities, selected);
    Assert::AreEqual(std::string("Retreat: mixed"), content.buttons.back().label);
    entities[0].retreat = Outpost::RetreatThreshold::Never;
    const std::vector<Outpost::EntityId> alone{Outpost::EntityId{9}};
    content = Outpost::Hud::Describe(Newest(), entities, alone);
    Assert::AreEqual(std::string("Never retreat"), content.buttons.back().label);
    Assert::IsTrue(content.buttons.back().action.retreat == Outpost::RetreatThreshold::Quarter, L"round to a quarter");
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

  // Interface plan 2, task UI3.2: the Ore, status, territory and alerts panels are one width, the column's, whatever the
  // status panel holds, so that the column's edge stands still; no narrower than the alerts' 380 units. Research's progress
  // and the fleet against its cap are bars, filled for their share, at nothing, half and the whole: research's under its
  // name, across the panel, and the fleet's beside its label, short of its figure.
  TEST_METHOD(KeepsTheColumnOneWidth)
  {
    using StatusBar = Outpost::Hud::StatusBar;
    using StatusLine = Outpost::Hud::StatusLine;
    Outpost::Hud::Content content{.ore = 0, .oreIncomeHundredthsPerSecond = 650};
    content.researchNames = {"Hull Plating", "Mass Driver Calibration"};
    content.territory = Outpost::Hud::Territory{.ownNodes = 3, .enemyNodes = 2, .nodes = 25, .cap = 10};
    content.alerts = {{"Enemy ships in South", {}}};
    // The bodies of the panels at the left margin.
    const auto columnOf = [&content](std::vector<StatusLine> _status)
    {
      content.status = std::move(_status);
      const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
      std::vector<float> widths;
      for (const Outpost::Hud::Rect& panel : layout.panels)
      {
        if (std::abs(panel.left - 16.0f) < 0.01f && panel.color.x == 0.009f && panel.color.y == 0.013f && panel.color.z == 0.024f)
          widths.push_back(panel.width);
      }
      return widths;
    };
    const std::vector<float> bare = columnOf({});
    Assert::AreEqual(size_t{3}, bare.size(), L"the Ore, the territory and the alerts");
    Assert::IsTrue(bare.front() >= 380.0f);
    const std::vector<std::vector<StatusLine>> statuses{
      {{.runs = {{.text = "Research Lab "}, {.text = "IDLE", .chip = true}}}},
      {{.runs = {{.text = "Hull Plating"}}, .bar = StatusBar{.share = 0.5f, .figure = "+2", .under = true}},
       {.runs = {{.text = "Shipyards \xC2\xB7 BUILDING 1 \xC2\xB7 WAITING 0 \xC2\xB7 "}, {.text = "IDLE 2", .chip = true}}},
       {.runs = {{.text = "Fleet"}}, .bar = StatusBar{.share = 0.5f, .figure = "15 / 30"}}},
      {{.runs = {{.text = "Mass Driver Calibration"}, {.text = " \xC2\xB7 waiting for Ore"}}, .bar = StatusBar{.under = true}},
       {.runs = {{.text = "Shipyards at the fleet cap \xC2\xB7 Station upgrading, 62%"}}},
       {.runs = {{.text = "Fleet"}}, .bar = StatusBar{.share = 1.0f, .figure = "30 / 30"}}}};
    for (const std::vector<StatusLine>& status : statuses)
    {
      const std::vector<float> widths = columnOf(status);
      Assert::AreEqual(size_t{4}, widths.size(), L"and the status panel");
      Assert::IsTrue(std::ranges::all_of(widths, [&](float _width) { return std::abs(_width - bare.front()) < 0.01f; }), L"one width");
    }

    const auto isTrack = [](const Outpost::Hud::Rect& _panel) { return _panel.color.x == 0.004f && _panel.color.z == 0.009f; };
    const auto isFill = [](const Outpost::Hud::Rect& _panel) { return _panel.color.x == 0.17f && _panel.color.z == 0.58f; };
    for (const float share : {0.0f, 0.5f, 1.0f})
    {
      content.status = {{.runs = {{.text = "Hull Plating"}}, .bar = StatusBar{.share = share, .figure = "+2", .under = true}},
                        {.runs = {{.text = "Fleet"}}, .bar = StatusBar{.share = share, .figure = "15 / 30"}}};
      const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
      std::vector<Outpost::Hud::Rect> tracks;
      std::ranges::copy_if(layout.panels, std::back_inserter(tracks), isTrack);
      Assert::AreEqual(size_t{2}, tracks.size());
      for (const Outpost::Hud::Rect& track : tracks)
      {
        const auto fill = std::ranges::find_if(layout.panels, [&](const Outpost::Hud::Rect& _panel)
                                               { return isFill(_panel) && _panel.top == track.top && _panel.left == track.left; });
        Assert::AreEqual(share > 0.0f, fill != layout.panels.end(), L"a fill only for a share");
        if (fill != layout.panels.end())
          Assert::AreEqual(share * track.width, fill->width, 0.01f);
      }
      const Outpost::Hud::Text& name = TextOf(layout, "Hull Plating");
      const Outpost::Hud::Text& fleet = TextOf(layout, "Fleet");
      const Outpost::Hud::Text& figure = TextOf(layout, "15 / 30");
      const Outpost::Hud::Rect& under = tracks[0].top < tracks[1].top ? tracks[0] : tracks[1];
      const Outpost::Hud::Rect& beside = tracks[0].top < tracks[1].top ? tracks[1] : tracks[0];
      Assert::IsTrue(under.top >= name.top + 20.0f && under.top < fleet.top && under.left <= name.left, L"under the name");
      Assert::IsTrue(beside.top > fleet.top && beside.top < fleet.top + 22.0f && beside.left > fleet.left, L"beside the label");
      Assert::IsTrue(beside.left + beside.width < figure.left, L"short of its figure");
    }
  }

  // ADR-059: the alerts under the territory, the newest a warning chip (ADR-085 decision 1), and a mark at each on the
  // minimap.
  TEST_METHOD(ListsTheAlerts)
  {
    Outpost::Hud::Content content{.ore = 0, .selection = {}, .mapSizeMeters = 2000.0f};
    content.alerts = {{"Relay suppressed: South", {.xMeters = 0.0f, .zMeters = -500.0f}}, {"Enemy ships in West", {}}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const auto newest = std::ranges::find(layout.texts, std::string("Relay suppressed: South"), &Outpost::Hud::Text::text);
    const auto older = std::ranges::find(layout.texts, std::string("Enemy ships in West"), &Outpost::Hud::Text::text);
    Assert::IsTrue(newest != layout.texts.end() && older != layout.texts.end());
    Assert::IsTrue(newest->top < older->top, L"newest first");
    Assert::IsTrue(IsChip(layout, *newest) && !IsChip(layout, *older), L"the newest a chip");
    Assert::AreEqual(older->left, newest->left, 0.01f, L"its words in line with the others'");
    const DirectX::XMFLOAT2 at = layout.MinimapPixelOf({.xMeters = 0.0f, .zMeters = -500.0f});
    Assert::IsTrue(
      std::ranges::any_of(layout.panels, [&](const Outpost::Hud::Rect& _panel)
                          { return _panel.left < at.x && _panel.left + _panel.width > at.x && _panel.top < at.y && _panel.height < 3.0f; }),
      L"a mark round the alert's place");
  }

  // Phase 5 design §7, §11: the orders window lists the seat's scheduled orders, the first ORDERS_SHOWN with how many ships
  // wait on each and how many more there are; and lays out the form, a row a field with the arrows that step it either way,
  // when the order fires, and the button that gives it to the selection's warships, which says why it cannot while it
  // cannot.
  TEST_METHOD(LaysOutTheOrdersWindow)
  {
    Outpost::Snapshot newest = OrdersSnapshot();
    for (std::uint32_t id = 1; id <= Outpost::Hud::ORDERS_SHOWN + 2; ++id)
    {
      newest.scheduled.push_back({.id = id,
                                  .ships = {Outpost::EntityId{100}, Outpost::EntityId{101}},
                                  .trigger = {.kind = Outpost::ScheduledTriggerKind::TimeOfDay, .utcSeconds = std::int64_t{2} * 3600},
                                  .action = {.kind = Outpost::ScheduledActionKind::AttackMove}});
    }
    const Outpost::PlayerClock clock;
    const std::chrono::sys_seconds now{std::chrono::hours{1}};
    Outpost::OrderForm form;
    form.Update(newest, newest.entities);
    Outpost::Hud::OrdersPanel panel = Outpost::Hud::DescribeOrders(newest, newest.entities, {}, form, clock, now);
    Assert::AreEqual(Outpost::Hud::ORDERS_SHOWN, panel.scheduled.size());
    Assert::AreEqual(std::size_t{2}, panel.more);
    Assert::AreEqual(std::string("At 02:00, attack-move to North \xC2\xB7 2 ships"), panel.scheduled.front());
    const auto labelsOf = [](const Outpost::Hud::OrdersPanel& _panel)
    {
      std::vector<std::string> labels;
      labels.reserve(_panel.rows.size());
      for (const Outpost::Hud::OrderRow& row : _panel.rows)
        labels.push_back(row.label);
      return labels;
    };
    Assert::IsTrue(labelsOf(panel) == std::vector<std::string>{"WHEN", "HOUR", "MINUTE", "DO", "WHERE", "UNLESS"});
    Assert::AreEqual(std::string("02"), panel.rows[1].value);
    Assert::AreEqual(std::string("Never"), panel.rows.back().value);
    Assert::AreEqual(std::string("Fires at 02:00 on your clock, in 1:00:00"), panel.fires);
    Assert::IsFalse(panel.give.enabled);
    Assert::AreEqual(std::string("SELECT WARSHIPS"), panel.give.note);

    Outpost::Hud::Content content;
    content.orders = panel;
    Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(std::size_t{1}, layout.windows.size());
    Assert::IsTrue(layout.windows[0].kind == Outpost::WindowKind::Orders);
    for (const Outpost::Hud::OrderRow& row : panel.rows)
    {
      for (const int step : {-1, 1})
      {
        Assert::IsTrue(std::ranges::any_of(layout.actions,
                                           [&](const auto& _action) {
                                             return _action.second == Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::StepOrder,
                                                                                           .field = row.field,
                                                                                           .step = step};
                                           }),
                       std::format(L"{} steps {}", std::wstring(winrt::to_hstring(row.label)), step).c_str());
      }
    }
    const auto gives = [&layout]
    {
      return std::ranges::any_of(layout.actions,
                                 [](const auto& _action) { return _action.second.kind == Outpost::Hud::ActionKind::GiveOrder; });
    };
    Assert::IsFalse(gives(), L"nothing to give it to");
    for (const std::string text : {"SELECT WARSHIPS", "and 2 more", "Fires at 02:00 on your clock, in 1:00:00"})
      Assert::IsTrue(std::ranges::find(layout.texts, text, &Outpost::Hud::Text::text) != layout.texts.end());

    // An event trigger watches a sector, and an attack takes a target there.
    form.Step(Outpost::OrderField::Trigger, 1, newest, newest.entities);
    form.Step(Outpost::OrderField::Action, 1, newest, newest.entities);
    const std::vector<Outpost::EntityId> selected{Outpost::EntityId{100}};
    panel = Outpost::Hud::DescribeOrders(newest, newest.entities, selected, form, clock, now);
    Assert::IsTrue(labelsOf(panel) == std::vector<std::string>{"WHEN", "WATCHING", "DO", "WHERE", "TARGET", "UNLESS"});
    Assert::AreEqual(std::string("North"), panel.rows[1].value);
    Assert::AreEqual(std::string("Relay (1 of 1)"), panel.rows[4].value);
    Assert::AreEqual(std::string("Fires once, the first time it happens"), panel.fires);
    Assert::IsTrue(panel.give.enabled);
    Assert::AreEqual(std::string("Give to 1 ship|"), panel.give.label);
    content.orders = panel;
    layout = Lay(content, 1920, 1080);
    Assert::IsTrue(gives());
  }

  // Design §11: the selection's panel names the scheduled order its ship waits on, whole.
  TEST_METHOD(NamesAShipsPendingOrder)
  {
    Outpost::Snapshot newest = OrdersSnapshot();
    newest.scheduled = {{.id = 4,
                         .ships = {Outpost::EntityId{100}},
                         .trigger = {.kind = Outpost::ScheduledTriggerKind::TimeOfDay, .utcSeconds = std::int64_t{2} * 3600},
                         .action = {.kind = Outpost::ScheduledActionKind::HoldSector}}};
    newest.entities[0].scheduledOrder = 4;
    const std::vector<Outpost::EntityId> selected{Outpost::EntityId{100}};
    Outpost::Hud::Content content = Outpost::Hud::Describe(newest, newest.entities, selected);
    Assert::IsFalse(content.selection.empty());
    const std::string pending = Outpost::PendingOrderLine(selected, newest.entities, newest, Outpost::PlayerClock()).value_or("");
    Assert::AreEqual(std::string("Scheduled: at 02:00, hold North"), pending);
    content.selection.push_back(pending);
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::IsTrue(std::ranges::find(layout.texts, pending, &Outpost::Hud::Text::text) != layout.texts.end(), L"whole");
  }

  // Design §11: on taking its seat again the player is told what happened while it was away, in a window of its own: how
  // long, what was built and lost, the sectors gained and lost, and each order that fired.
  TEST_METHOD(SaysWhatHappenedWhileAway)
  {
    Outpost::Snapshot newest = OrdersSnapshot();
    newest.sectors.push_back(newest.sectors.front());
    newest.sectors.back().id = 2;
    newest.sectors.back().nameUtf8 = "South";
    const Outpost::AwayReport report{
      .sinceTick = 100,
      .shipsBuilt = 3,
      .structuresBuilt = 1,
      .shipsLost = 0,
      .structuresLost = 2,
      .sectorsGained = {1},
      .sectorsLost = {2},
      .ordersFired = {{.kind = Outpost::EventKind::OrderFired, .sector = 1, .action = Outpost::ScheduledActionKind::AttackMove},
                      {.kind = Outpost::EventKind::OrderFired,
                       .sector = 2,
                       .action = Outpost::ScheduledActionKind::Move,
                       .outcome = Outpost::OrderOutcome::HeldInstead}}};
    const Outpost::Hud::AwayPanel panel = Outpost::Hud::DescribeAway(newest, report, 100 + (20 * 3725), 20);
    Assert::AreEqual(std::string("Away for 1:02:05"), panel.since);
    Assert::IsTrue(panel.lines == std::vector<std::string>{"Built 3 ships and 1 structure", "Lost no ships and 2 structures",
                                                           "Sectors gained: North", "Sectors lost: South",
                                                           "Order fired: attack-move to North", "Order held back: move to South"});

    Outpost::Hud::Content content;
    Assert::IsTrue(Lay(content, 1920, 1080).windows.empty(), L"nothing to tell, no window");
    content.away = panel;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(std::size_t{1}, layout.windows.size());
    Assert::IsTrue(layout.windows[0].kind == Outpost::WindowKind::Away);
    for (const std::string& line : panel.lines)
      Assert::IsTrue(std::ranges::find(layout.texts, line, &Outpost::Hud::Text::text) != layout.texts.end());
  }

  // ADR-057: under the nodes, each side's tickets, the player's in its color and the enemy's in theirs. Interface plan 2,
  // task UI3.3: under the tickets, who the drain takes from, at what a minute, and when they run out, the drains their
  // tickets last rounded up, times the interval; a warning chip when it is the player, and "No drain" level on nodes.
  TEST_METHOD(ShowsTheTickets)
  {
    const Outpost::PlayerId enemy{2};
    Outpost::Snapshot newest = Newest();
    newest.sectors = {{.id = 1, .maxXMeters = 100.0f, .maxZMeters = 100.0f, .holder = PLAYER}};
    newest.tickets = {{.player = PLAYER, .tickets = 1000}, {.player = enemy, .tickets = 642}};
    newest.drainIntervalSeconds = 3.0;
    newest.drainTicketsPerNodeDifference = 1;
    const auto territoryOf = [&newest]() { return Outpost::Hud::Describe(newest, {}, {}).territory.value_or(Outpost::Hud::Territory{}); };
    // Ahead by a node, at the review's first screenshot's figures: the enemy loses 20 a minute, out in 32:06.
    Outpost::Hud::Territory territory = territoryOf();
    Assert::IsTrue(territory.ownTickets == 1000 && territory.enemyTickets == 642);
    Assert::AreEqual(std::string("Enemy -20 a minute \xC2\xB7 out in 32:06"), territory.drain);
    Assert::IsFalse(territory.drainWarns);
    const Outpost::Hud::Layout layout = Lay(Outpost::Hud::Describe(newest, {}, {}), 1920, 1080);
    const Outpost::Hud::Text& tickets = TextOf(layout, "Tickets");
    const Outpost::Hud::Text& drain = TextOf(layout, territory.drain);
    Assert::IsTrue(TextOf(layout, "642").left > TextOf(layout, "1,000 : ").left, L"the enemy's figure stands at the right");
    Assert::IsTrue(TextOf(layout, "Nodes").top < tickets.top && tickets.top < drain.top, L"nodes, tickets, then the drain");
    Assert::IsFalse(IsChip(layout, drain), L"the enemy's loss is plain");

    // Level on nodes: no drain.
    newest.sectors.push_back({.id = 2, .minXMeters = 100.0f, .maxXMeters = 200.0f, .maxZMeters = 100.0f, .holder = enemy});
    Assert::AreEqual(std::string("No drain"), territoryOf().drain);

    // Behind by three, at the fifth screenshot's time: the player loses 60 a minute and is out in 12:09, a warning chip.
    newest.sectors = {{.id = 1, .maxXMeters = 100.0f, .maxZMeters = 100.0f, .holder = enemy},
                      {.id = 2, .minXMeters = 100.0f, .maxXMeters = 200.0f, .maxZMeters = 100.0f, .holder = enemy},
                      {.id = 3, .minXMeters = 200.0f, .maxXMeters = 300.0f, .maxZMeters = 100.0f, .holder = enemy}};
    newest.tickets[0].tickets = 729;
    territory = territoryOf();
    Assert::AreEqual(std::string("You -60 a minute \xC2\xB7 out in 12:09"), territory.drain);
    Assert::IsTrue(territory.drainWarns);
    const Outpost::Hud::Layout behind = Lay(Outpost::Hud::Describe(newest, {}, {}), 1920, 1080);
    Assert::IsTrue(IsChip(behind, TextOf(behind, territory.drain)), L"the player's own loss is a chip");
    newest.tickets[0].tickets = 728;
    Assert::AreEqual(std::string("You -60 a minute \xC2\xB7 out in 12:09"), territoryOf().drain, L"a part drain is a whole one");

    // Without the drain's numbers, no row.
    newest.drainIntervalSeconds = 0.0;
    Assert::IsTrue(territoryOf().drain.empty());
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

  // Task 6.2: the main menu starts a skirmish, at each difficulty from the easiest down (ADR-065), or quits, from the middle
  // of the screen at any size.
  TEST_METHOD(LaysOutTheMenu)
  {
    for (const auto& [width, height] : std::array<std::pair<std::uint32_t, std::uint32_t>, 2>{{{1920, 1080}, {1280, 720}}})
    {
      const Outpost::Hud::Layout layout = LayMenu(width, height);
      Assert::AreEqual(size_t{4}, layout.actions.size());
      const std::array<Outpost::Difficulty, 3> difficulties{Outpost::Difficulty::Easy, Outpost::Difficulty::Normal,
                                                            Outpost::Difficulty::Hard};
      for (std::size_t i = 0; i < difficulties.size(); ++i)
      {
        Assert::IsTrue(layout.actions[i].second ==
                       Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::StartSkirmish, .difficulty = difficulties[i]});
        if (i > 0)
          Assert::IsTrue(layout.actions[i].first.top > layout.actions[i - 1].first.top, L"each under the last");
      }
      Assert::IsTrue(layout.actions[3].second.kind == Outpost::Hud::ActionKind::Quit);
      const Outpost::Hud::Rect& start = layout.actions[0].first;
      Assert::IsTrue(start.left < static_cast<float>(width) / 2.0f && start.left + start.width > static_cast<float>(width) / 2.0f);
      Assert::IsTrue(layout.actions[3].first.top > layout.actions[2].first.top, L"Quit last");
      Assert::IsTrue(layout.ActionAt(start.left + 5.0f, start.top + 5.0f) ==
                     Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::StartSkirmish, .difficulty = Outpost::Difficulty::Easy});
      Assert::IsTrue(std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "Outpost Commander"; }));
      Assert::IsFalse(layout.minimap.width > 0.0f);
    }
  }

  // ADR-078: with a join file, the menu offers to join its world, between the skirmishes and Quit; and it says, in the
  // warning's color, wrapped to its width, why the last game ended.
  TEST_METHOD(OffersAWorldAndSaysWhyTheLastGameEnded)
  {
    const std::string notice = "The connection to the server was lost: The seat was taken by another connection with its token.";
    const Outpost::Hud::Layout layout = LayMenu(1920, 1080, 1.0f, {.joinWorld = true, .notice = notice});
    Assert::AreEqual(size_t{5}, layout.actions.size());
    Assert::IsTrue(layout.actions[3].second.kind == Outpost::Hud::ActionKind::JoinWorld, L"Join world after the skirmishes");
    Assert::IsTrue(layout.actions[4].second.kind == Outpost::Hud::ActionKind::Quit, L"and Quit last");
    Assert::IsTrue(layout.actions[3].first.top > layout.actions[2].first.top && layout.actions[4].first.top > layout.actions[3].first.top);

    std::vector<const Outpost::Hud::Text*> lines;
    for (const Outpost::Hud::Text& text : layout.texts)
    {
      if (notice.find(text.text) != std::string::npos && !text.text.empty() && text.text != "Outpost Commander")
        lines.push_back(&text);
    }
    Assert::IsTrue(lines.size() >= 2, L"the notice wraps");
    std::string joined;
    for (const Outpost::Hud::Text* line : lines)
    {
      joined += (joined.empty() ? "" : " ") + line->text;
      Assert::IsTrue(line->color.x == 1.0f && line->color.y == 0.5f, L"in the warning's color");
      Assert::IsTrue(line->top < layout.actions[0].first.top, L"above the buttons");
    }
    Assert::AreEqual(notice, joined, L"every word of it");

    const Outpost::Hud::Layout plain = LayMenu(1920, 1080);
    Assert::AreEqual(size_t{4}, plain.actions.size(), L"without a join file, no Join world");
  }
  // ADR-066: under the Ore, a line for the Research Lab once the player has a finished one, and one for the Shipyards once
  // the first is finished: what the Lab researches and how far it has come, that it waits for Ore, or that it is idle; and
  // how many Shipyards build, wait for Ore and stand idle. An idle line says IDLE, in a chip, and a click on a
  // line opens its window: research, or production at the first idle Shipyard, or at the first Shipyard when none is.
  TEST_METHOD(ShowsWhatProductionAndResearchAreDoing)
  {
    Outpost::Snapshot newest = Newest();
    newest.research = {{.id = Outpost::ResearchTopicId{2}, .nameUtf8 = "Hull Plating"}};
    // A Small hull any Shipyard builds and a Medium one of level 2, and a design of each.
    newest.hulls = {{.id = Outpost::HullId{1}, .nameUtf8 = "Small", .commandPoints = 1},
                    {.id = Outpost::HullId{2}, .nameUtf8 = "Medium", .shipyardLevel = 2, .commandPoints = 2}};
    newest.designs[0].hull = Outpost::HullId{1};
    newest.designs[0].cost = 100;
    newest.designs[1].hull = Outpost::HullId{2};
    newest.designs[1].cost = 200;
    const auto structure = [](std::uint32_t _id, Outpost::StructureKind _kind, std::uint32_t _number)
    {
      return Outpost::EntityView{.id = Outpost::EntityId{_id},
                                 .kind = Outpost::EntityKind::Structure,
                                 .owner = PLAYER,
                                 .structure = _kind,
                                 .shipyardNumber = _number};
    };
    const auto statusOf = [&newest](const std::vector<Outpost::EntityView>& _entities)
    { return Outpost::Hud::Describe(newest, _entities, {}).status; };

    std::vector<Outpost::EntityView> entities{structure(20, Outpost::StructureKind::CommandStation, 0)};
    Assert::IsTrue(statusOf(entities).empty(), L"no Lab and no Shipyard");
    Outpost::EntityView lab = structure(40, Outpost::StructureKind::ResearchLab, 0);
    lab.builtPermille = 500;
    entities.push_back(lab);
    Assert::IsTrue(statusOf(entities).empty(), L"not until the Lab is finished");

    const Outpost::Hud::Action research{.kind = Outpost::Hud::ActionKind::OpenResearch, .producer = lab.id};
    entities.back().builtPermille = Outpost::PERMILLE;
    std::vector<Outpost::Hud::StatusLine> status = statusOf(entities);
    Assert::AreEqual(size_t{1}, status.size());
    Assert::AreEqual(std::string("Research Lab IDLE"), status[0].Text());
    Assert::IsTrue(status[0].Warns() && status[0].action == research);
    // Researching: the topic over a bar of how far it has come, and how many more are queued at its end (task UI3.2).
    entities.back().research = {Outpost::ResearchTopicId{2}};
    status = statusOf(entities);
    Assert::AreEqual(std::string("Hull Plating \xC2\xB7 waiting for Ore"), status[0].Text());
    Assert::IsTrue(status[0].bar == Outpost::Hud::StatusBar{.share = 0.0f, .under = true});
    entities.back().jobPermille = 620;
    entities.back().research.push_back(Outpost::ResearchTopicId{2});
    status = statusOf(entities);
    Assert::AreEqual(std::string("Hull Plating"), status[0].Text());
    Assert::IsTrue(status[0].bar == Outpost::Hud::StatusBar{.share = 0.62f, .figure = "+1", .under = true});
    Assert::IsFalse(status[0].Warns());
    entities.back().research.pop_back();

    // An idle Lab with no topic open to it says so plainly: every topic researched, or one whose prerequisite is not, or of
    // a tier the Lab has not opened.
    entities.back().research.clear();
    entities.back().jobPermille = 0;
    newest.research = {{.id = Outpost::ResearchTopicId{2}, .nameUtf8 = "Hull Plating", .researched = true},
                       {.id = Outpost::ResearchTopicId{3}, .nameUtf8 = "Fusion Drive", .prerequisites = {Outpost::ResearchTopicId{4}}},
                       {.id = Outpost::ResearchTopicId{4}, .nameUtf8 = "Relay Archives", .tier = 2}};
    status = statusOf(entities);
    Assert::AreEqual(std::string("Research: every open topic done"), status[0].Text());
    Assert::IsTrue(!status[0].Warns() && status[0].action == research);
    newest.researchTier = 2;
    Assert::IsTrue(statusOf(entities)[0].Warns(), L"tier 2 open, Relay Archives is open to it");
    newest.researchTier = 1;
    newest.research = {{.id = Outpost::ResearchTopicId{2}, .nameUtf8 = "Hull Plating"}};
    entities.back().research = {Outpost::ResearchTopicId{2}};
    entities.back().jobPermille = 620;

    // Shipyards 01 and 03 build, 02 waits for Ore, and 04 and 05 stand idle: production opens at 04, the first idle by
    // number, wherever it stands among the entities.
    Outpost::EntityView building = structure(31, Outpost::StructureKind::Shipyard, 1);
    building.queue = {{.design = SWARM}};
    building.jobPermille = 100;
    Outpost::EntityView waiting = structure(32, Outpost::StructureKind::Shipyard, 2);
    waiting.queue = {{.design = SWARM}};
    Outpost::EntityView third = building;
    third.id = Outpost::EntityId{33};
    third.shipyardNumber = 3;
    entities.push_back(structure(35, Outpost::StructureKind::Shipyard, 5));
    entities.push_back(waiting);
    entities.push_back(structure(34, Outpost::StructureKind::Shipyard, 4));
    entities.push_back(building);
    entities.push_back(third);
    Outpost::EntityView theirs = structure(36, Outpost::StructureKind::Shipyard, 0);
    theirs.owner = Outpost::PlayerId{2};
    entities.push_back(theirs);
    status = statusOf(entities);
    Assert::AreEqual(size_t{2}, status.size(), L"no fleet line without a cap");
    Assert::AreEqual(std::string("Shipyards \xC2\xB7 BUILDING 2 \xC2\xB7 WAITING 1 \xC2\xB7 IDLE 2"), status[1].Text(), L"not the enemy's");
    Assert::IsTrue(status[1].Warns());
    Assert::IsTrue(status[1].action ==
                   Outpost::Hud::Action{.kind = Outpost::Hud::ActionKind::OpenProduction, .producer = Outpost::EntityId{34}});

    // Idle, but no saved design the player has the Ore for: counted plainly.
    newest.ore = 50;
    status = statusOf(entities);
    Assert::AreEqual(std::string("Shipyards \xC2\xB7 BUILDING 2 \xC2\xB7 WAITING 1 \xC2\xB7 IDLE 2"), status[1].Text());
    Assert::IsFalse(status[1].Warns(), L"nothing an idle Shipyard could start");
    // Ore only for a Medium, which no Shipyard of level 1 builds; one of level 2 could start it.
    newest.ore = 150;
    std::swap(newest.designs[0].cost, newest.designs[1].cost);
    Assert::IsFalse(statusOf(entities)[1].Warns(), L"a hull no idle Shipyard builds");
    const auto fourth = std::ranges::find(entities, Outpost::EntityId{34}, &Outpost::EntityView::id);
    fourth->level = 2;
    Assert::IsTrue(statusOf(entities)[1].Warns(), L"a Shipyard of level 2 builds it");
    fourth->level = 1;
    std::swap(newest.designs[0].cost, newest.designs[1].cost);
    newest.ore = 12345;

    // None idle: no warning, and production opens at the first Shipyard.
    std::erase_if(entities, [](const Outpost::EntityView& _entity) { return _entity.queue.empty() && _entity.shipyardNumber > 0; });
    status = statusOf(entities);
    Assert::AreEqual(std::string("Shipyards \xC2\xB7 BUILDING 2 \xC2\xB7 WAITING 1 \xC2\xB7 IDLE 0"), status[1].Text(),
                     L"every count, in its place");
    Assert::IsFalse(status[1].Warns());
    Assert::IsTrue(status[1].action.producer == building.id);

    // Laid out: under the Ore, the idle Shipyard's IDLE a chip after the counts, and a click on each line opens its window.
    entities.push_back(structure(34, Outpost::StructureKind::Shipyard, 4));
    const Outpost::Hud::Content content = Outpost::Hud::Describe(newest, entities, {});
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const Outpost::Hud::Text& labLine = TextOf(layout, content.status[0].Text());
    const Outpost::Hud::Text& yardsLine = TextOf(layout, content.status[1].runs.front().text);
    const Outpost::Hud::Text& idle = TextOf(layout, "IDLE 1");
    Assert::IsTrue(labLine.top > 60.0f && labLine.left < 40.0f && yardsLine.top > labLine.top, L"under the Ore");
    Assert::IsTrue(idle.top == yardsLine.top && idle.left > yardsLine.left, L"on the Shipyards' line, after the counts");
    Assert::IsTrue(IsChip(layout, idle) && !IsChip(layout, yardsLine) && !IsChip(layout, labLine), L"the idle Shipyard warns");
    Assert::IsTrue(layout.ActionAt(labLine.left + 4.0f, labLine.top + 6.0f) == research);
    Assert::IsTrue(layout.ActionAt(yardsLine.left + 4.0f, yardsLine.top + 6.0f) == content.status[1].action);
    Assert::IsTrue(layout.ActionAt(idle.left + 4.0f, idle.top + 6.0f) == content.status[1].action, L"the chip too");
  }

  // Phase 4 design §5: a Shipyard whose front warship does not fit under the cap says it waits for the cap, not for Ore, in
  // its selection panel and its production window. Interface plan 2, task UI3.1: on the status line the cap is then what
  // holds the Shipyards back, as it is when the fleet has room for none of the saved designs. The line says so, plainly,
  // with what the Command Station's next level adds, or how far its upgrade has come, and a click selects the station.
  // Otherwise the line counts the Shipyards and ends with the fleet against its cap; without a cap it says nothing of it.
  // The Command Station's next level names the cap it gives.
  TEST_METHOD(ShowsTheFleetAgainstItsCap)
  {
    Outpost::Snapshot newest = Newest();
    newest.designs = {{.id = SWARM, .nameUtf8 = "Small+Ion+Mass Driver", .hull = Outpost::HullId{1}},
                      {.id = LINE, .nameUtf8 = "Medium+Ion+Lance", .hull = Outpost::HullId{2}}};
    newest.hulls = {{.id = Outpost::HullId{1}, .nameUtf8 = "Small", .commandPoints = 1},
                    {.id = Outpost::HullId{2}, .nameUtf8 = "Medium", .commandPoints = 2}};
    newest.commandPoints = 11;
    newest.fleetCap = 12;
    Outpost::EntityView swarmYard{.id = Outpost::EntityId{31},
                                  .kind = Outpost::EntityKind::Structure,
                                  .owner = PLAYER,
                                  .structure = Outpost::StructureKind::Shipyard,
                                  .hitPointsHundredths = 250000,
                                  .maxHitPointsHundredths = 250000,
                                  .shipyardNumber = 1,
                                  .queue = {{.design = SWARM}}};
    Outpost::EntityView lineYard = swarmYard;
    lineYard.id = Outpost::EntityId{32};
    lineYard.shipyardNumber = 2;
    lineYard.queue = {{.design = LINE}, {.design = SWARM}};
    newest.structureTypes = {
      {.structure = Outpost::StructureKind::CommandStation,
       .nameUtf8 = "Command Station",
       .levels = {{.cost = 300, .buildSeconds = 45.0, .maxHitPointsHundredths = 600000, .nodes = 4, .commandPoints = 20},
                  {.cost = 500, .buildSeconds = 60.0, .maxHitPointsHundredths = 700000, .nodes = 6, .commandPoints = 30}}}};
    Outpost::EntityView station{.id = Outpost::EntityId{20},
                                .kind = Outpost::EntityKind::Structure,
                                .owner = PLAYER,
                                .structure = Outpost::StructureKind::CommandStation,
                                .hitPointsHundredths = 500000,
                                .maxHitPointsHundredths = 500000};
    std::vector<Outpost::EntityView> entities{swarmYard, lineYard, station};
    const auto statusOf = [&]() { return Outpost::Hud::Describe(newest, entities, {}).status; };
    const Outpost::Hud::Action select{.kind = Outpost::Hud::ActionKind::Select, .entity = station.id};

    // A Small fits at 11 of 12, so the first waits for Ore; a Medium does not, so the second waits for the cap, and the cap
    // is what holds the Shipyards back. The station's next level lifts it by 8. Under it, the fleet as a bar against the
    // cap, its click the Shipyards' line's.
    std::vector<Outpost::Hud::StatusLine> status = statusOf();
    Assert::AreEqual(size_t{2}, status.size());
    Assert::AreEqual(std::string("Shipyards at the fleet cap \xC2\xB7 Station L2: +8"), status[0].Text());
    Assert::IsTrue(!status[0].Warns() && status[0].action == select, L"plain, and a click goes to the station");
    Assert::AreEqual(std::string("Fleet"), status[1].Text());
    Assert::IsTrue(status[1].bar == Outpost::Hud::StatusBar{.share = 11.0f / 12.0f, .figure = "11 / 12"} && status[1].action == select);
    // While the station is upgrading, how far it has come; at its top level, the cap alone.
    entities[2].upgradePermille = 620;
    Assert::AreEqual(std::string("Shipyards at the fleet cap \xC2\xB7 Station upgrading, 62%"), statusOf()[0].Text());
    entities[2].upgradePermille.reset();
    entities[2].level = 3;
    Assert::AreEqual(std::string("Shipyards at the fleet cap"), statusOf()[0].Text());
    entities[2].level = 1;
    // No Shipyard waits for the cap, but the fleet has room for none of the saved designs: at the cap too.
    newest.commandPoints = 12;
    entities[0].queue.clear();
    entities[1].queue.clear();
    status = statusOf();
    Assert::AreEqual(std::string("Shipyards at the fleet cap \xC2\xB7 Station L2: +8"), status[0].Text());
    Assert::IsTrue(status[0].action == select, L"idle at the cap, no chip and the station");
    Assert::AreEqual(1.0f, status[1].bar.value_or(Outpost::Hud::StatusBar{}).share, L"a full fleet");
    // Room for a Small: the counts, an idle Shipyard that could start one a chip, and the fleet under them.
    newest.commandPoints = 11;
    entities[0].queue = {{.design = SWARM}};
    status = statusOf();
    Assert::AreEqual(std::string("Shipyards \xC2\xB7 BUILDING 0 \xC2\xB7 WAITING 1 \xC2\xB7 IDLE 1"), status[0].Text());
    Assert::IsTrue(status[0].Warns());
    const Outpost::Hud::Action production{.kind = Outpost::Hud::ActionKind::OpenProduction, .producer = lineYard.id};
    Assert::IsTrue(status[0].action == production && status[1].action == production);
    entities = {swarmYard, lineYard, station};
    const std::vector<std::string> selection = Outpost::Hud::Describe(newest, entities, std::vector{lineYard.id}).selection;
    Assert::IsTrue(
      std::ranges::find(selection, std::string("Building Medium+Ion+Lance \xC2\xB7 waiting for the fleet cap \xC2\xB7 +1 queued")) !=
      selection.end());
    Assert::IsTrue(Outpost::Hud::DescribeProduction(newest, &entities[1]).queue.front().capped);
    Assert::IsFalse(Outpost::Hud::DescribeProduction(newest, &entities[0]).queue.front().capped);

    // Without a cap, nothing is said of the fleet.
    newest.fleetCap = 0;
    status = statusOf();
    Assert::AreEqual(size_t{1}, status.size());
    Assert::AreEqual(std::string("Shipyards \xC2\xB7 BUILDING 0 \xC2\xB7 WAITING 2 \xC2\xB7 IDLE 0"), status[0].Text());

    // The station's next level gives a larger fleet.
    const std::vector<std::string> stationLines = Outpost::Hud::Describe(newest, std::vector{station}, std::vector{station.id}).selection;
    Assert::IsTrue(std::ranges::find(stationLines, std::string("L2: 4 nodes, fleet 20, 6,000 hit points")) != stationLines.end());
  }

  // ADR-066: the selection panel of the player's own finished Shipyard, Command Station or Research Lab says what its front
  // job is and how far it has come, or that it waits for Ore, with how many more are queued; or that it is idle. An enemy
  // structure's queue stays unshown (task 9.4).
  TEST_METHOD(SaysWhatASelectedProducerIsDoing)
  {
    Outpost::Snapshot newest = Newest();
    newest.research = {{.id = Outpost::ResearchTopicId{2}, .nameUtf8 = "Hull Plating"}};
    Outpost::EntityView yard{.id = Outpost::EntityId{30},
                             .kind = Outpost::EntityKind::Structure,
                             .owner = PLAYER,
                             .structure = Outpost::StructureKind::Shipyard,
                             .hitPointsHundredths = 250000,
                             .maxHitPointsHundredths = 250000};
    const auto lineOf = [&](const Outpost::EntityView& _entity)
    {
      const std::vector<std::string> selection = Outpost::Hud::Describe(newest, std::vector{_entity}, std::vector{_entity.id}).selection;
      return selection.size() > 2 ? selection[2] : std::string();
    };
    Assert::AreEqual(std::string("Idle"), lineOf(yard));
    yard.queue = {{.design = LINE}, {.design = SWARM}, {.design = SWARM}};
    Assert::AreEqual(std::string("Building Medium+Ion+Lance \xC2\xB7 waiting for Ore \xC2\xB7 +2 queued"), lineOf(yard));
    yard.jobPermille = 625;
    Assert::AreEqual(std::string("Building Medium+Ion+Lance \xC2\xB7 62% \xC2\xB7 +2 queued"), lineOf(yard));

    Outpost::EntityView station = yard;
    station.structure = Outpost::StructureKind::CommandStation;
    station.queue = {{.role = Outpost::ShipRole::Constructor}};
    Assert::AreEqual(std::string("Building Constructor \xC2\xB7 62%"), lineOf(station));

    Outpost::EntityView lab = yard;
    lab.structure = Outpost::StructureKind::ResearchLab;
    lab.queue.clear();
    lab.research = {Outpost::ResearchTopicId{2}};
    Assert::AreEqual(std::string("Researching Hull Plating \xC2\xB7 62%"), lineOf(lab));

    Outpost::EntityView theirs = yard;
    theirs.owner = Outpost::PlayerId{2};
    Assert::AreEqual(std::string(), lineOf(theirs), L"not the enemy's");
  }

  // Task 15.2: a button that opens a window shows its key at its right end, as a cap, an outline with the letter in the
  // label face; a Build button, which has its cost there, shows none.
  TEST_METHOD(ShowsAWindowsKeyOnItsButton)
  {
    const Outpost::Hud::Content content{.ore = 500,
                                        .buttons = {{.label = "Shipyard|300", .action = {.kind = Outpost::Hud::ActionKind::Build}},
                                                    {.label = "Production",
                                                     .action = {.kind = Outpost::Hud::ActionKind::OpenProduction},
                                                     .key = Outpost::KeyCap(Outpost::KEY_PRODUCTION)}}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(size_t{2}, layout.actions.size());
    const Outpost::Hud::Rect& build = layout.actions[0].first;
    const Outpost::Hud::Rect& production = layout.actions[1].first;
    const auto inside = [](const Outpost::Hud::Rect& _area, const Outpost::Hud::Text& _text)
    { return _area.Contains(_text.left, _text.top); };
    const auto cap = std::ranges::find(layout.texts, std::string("P"), &Outpost::Hud::Text::text);
    Assert::IsTrue(cap != layout.texts.end() && inside(production, *cap));
    Assert::IsTrue(cap->left > production.left + (production.width * 0.8f), L"at the right end");
    Assert::IsTrue(cap->typeface == Outpost::Hud::Typeface::Label);
    Assert::IsTrue(std::ranges::any_of(layout.panels,
                                       [&](const Outpost::Hud::Rect& _panel)
                                       {
                                         return _panel.width < 2.0f && _panel.height > 10.0f &&
                                                _panel.left > production.left + (production.width * 0.8f) &&
                                                _panel.left < production.left + production.width - 4.0f &&
                                                production.Contains(_panel.left, _panel.top);
                                       }),
                   L"the cap's outline, inside the button's right end");
    Assert::IsFalse(
      std::ranges::any_of(layout.texts, [&](const Outpost::Hud::Text& _text) { return inside(build, _text) && _text.text.size() == 1; }),
      L"no cap on a Build button");
    Assert::AreEqual(std::string("D"), Outpost::KeyCap(Outpost::KEY_DESIGNER));
    Assert::AreEqual(std::string("R"), Outpost::KeyCap(Outpost::KEY_RESEARCH));
  }

  // Task 15.4: a production or research window not yet moved opens in the first of the two places under the Ore that no
  // open window holds, production's and then research's, so that research alone opens at the margin, and the two stand
  // side by side in the order they opened. A moved window stays where it was left.
  TEST_METHOD(OpensAWindowWhereThereIsRoom)
  {
    Outpost::Hud::Content content;
    content.production = Outpost::Hud::ProductionPanel{.producer = "SHIPYARD 01", .hasProducer = true};
    content.laboratory = Outpost::Hud::ResearchPanel{.lab = "RESEARCH LAB", .hasLab = true};
    const auto frameOf = [&content](const Outpost::WindowManager& _windows, Outpost::WindowKind _kind)
    {
      const Outpost::Hud::Layout layout = Lay(content, 1920, 1080, {}, &_windows);
      const auto window = std::ranges::find(layout.windows, _kind, &Outpost::Hud::Window::kind);
      return window != layout.windows.end() ? window->frame : Outpost::Hud::Rect{};
    };
    constexpr float MARGIN_UNITS = 16.0f;

    Outpost::WindowManager windows;
    windows.Open(Outpost::WindowKind::Research);
    const Outpost::Hud::Rect alone = frameOf(windows, Outpost::WindowKind::Research);
    Assert::AreEqual(MARGIN_UNITS, alone.left, 0.01f, L"research alone at the margin");
    windows.Open(Outpost::WindowKind::Production);
    const Outpost::Hud::Rect second = frameOf(windows, Outpost::WindowKind::Production);
    Assert::AreEqual(alone.left + alone.width + MARGIN_UNITS, second.left, 0.01f, L"production beside it");
    Assert::AreEqual(alone.top, second.top, 0.01f);
    Assert::IsTrue(second.left + second.width <= 1920.0f - 16.0f - 818.0f, L"clear of the designer");
    Assert::AreEqual(alone.left, frameOf(windows, Outpost::WindowKind::Research).left, 0.01f, L"research keeps its place");

    Outpost::WindowManager inOrder;
    inOrder.Open(Outpost::WindowKind::Production);
    inOrder.Open(Outpost::WindowKind::Research);
    const Outpost::Hud::Rect production = frameOf(inOrder, Outpost::WindowKind::Production);
    Assert::AreEqual(MARGIN_UNITS, production.left, 0.01f);
    Assert::AreEqual(production.left + production.width + MARGIN_UNITS, frameOf(inOrder, Outpost::WindowKind::Research).left, 0.01f);

    // Moved, a window stays where it was left, and its place is free for the next to open.
    inOrder.Grab(Outpost::WindowKind::Production, {.xUnits = 700.0f, .yUnits = 600.0f}, {.xUnits = 700.0f, .yUnits = 600.0f});
    inOrder.Release();
    inOrder.Close(Outpost::WindowKind::Research);
    inOrder.Open(Outpost::WindowKind::Research);
    Assert::AreEqual(700.0f, frameOf(inOrder, Outpost::WindowKind::Production).left, 0.01f);
    Assert::AreEqual(MARGIN_UNITS, frameOf(inOrder, Outpost::WindowKind::Research).left, 0.01f);
  }

  // ADR-068 (task 16.1): the designer writes its queue as the other windows do, over its slots, and its ships built apart;
  // arrows 24 units square flank what they step, the designer's Shipyard and production's producer; research says where
  // its page is; and a window's body is opaque.
  TEST_METHOD(WritesTheWindowsHeaders)
  {
    // A Command Station beside Shipyard 01, so that production has another producer to step to and its arrows take clicks.
    Outpost::Snapshot newest = DesignerSnapshot();
    newest.entities.push_back({.id = Outpost::EntityId{20},
                               .kind = Outpost::EntityKind::Structure,
                               .owner = PLAYER,
                               .structure = Outpost::StructureKind::CommandStation});
    Outpost::Designer designer;
    designer.Update(newest);
    Outpost::Hud::Content content;
    content.designer = DesignerOf(newest, designer);
    content.production = Outpost::Hud::DescribeProduction(newest, &newest.entities.front());
    Outpost::Hud::ResearchPanel research{.lab = "RESEARCH LAB", .hasLab = true};
    for (std::uint32_t i = 1; i <= 21; ++i)
      research.topics.push_back({.name = std::format("Topic {}", i), .cost = 100});
    content.laboratory = research;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const auto areaOf = [&layout](Outpost::Hud::ActionKind _kind)
    {
      const auto found = std::ranges::find(layout.actions, _kind, [](const auto& _entry) { return _entry.second.kind; });
      return found != layout.actions.end() ? found->first : Outpost::Hud::Rect{};
    };
    // The designer is the last window laid out, and production the first: each one's texts are its own layer's.
    const Outpost::Hud::Window& designerWindow = layout.windows.back();
    const auto layerText = [&layout](std::size_t _layer, std::string_view _text)
    {
      const Outpost::Hud::Span span = layout.TextsOf(_layer);
      for (std::size_t t = span.first; t < span.end; ++t)
      {
        if (layout.texts[t].text == _text)
          return layout.texts[t];
      }
      Assert::Fail(std::wstring(winrt::to_hstring(_text)).c_str());
    };
    const std::size_t designerLayer = layout.windows.size();
    Assert::IsTrue(layerText(designerLayer, "QUEUE \xC2\xB7 2 / 5").top < layerText(designerLayer, "BUILT \xC2\xB7 4").top,
                   L"the queue over its slots, the ships built apart");

    // Production's producer between its arrows, each 24 units square.
    const Outpost::Hud::Text producer = layerText(1, "SHIPYARD 01");
    const Outpost::Hud::Rect previous = areaOf(Outpost::Hud::ActionKind::PreviousProducer);
    const Outpost::Hud::Rect next = areaOf(Outpost::Hud::ActionKind::NextProducer);
    Assert::AreEqual(24.0f, previous.width, 0.01f);
    Assert::AreEqual(24.0f, next.height, 0.01f);
    Assert::IsTrue(previous.left + previous.width < producer.left && next.left > producer.left, L"< SHIPYARD 01 >");
    Assert::IsTrue(std::abs((previous.top + 12.0f) - (producer.top + 15.0f)) < 6.0f, L"on the producer's line");
    // The designer's Shipyard between its arrows, in its title bar.
    const Outpost::Hud::Rect previousYard = areaOf(Outpost::Hud::ActionKind::PreviousShipyard);
    const Outpost::Hud::Rect nextYard = areaOf(Outpost::Hud::ActionKind::NextShipyard);
    const Outpost::Hud::Text yard = layerText(designerLayer, "SHIPYARD 01");
    Assert::IsTrue(designerWindow.titleBar.Contains(previousYard.left, previousYard.top) &&
                   designerWindow.titleBar.Contains(nextYard.left, nextYard.top));
    Assert::IsTrue(previousYard.left + previousYard.width < yard.left && nextYard.left > yard.left);
    Assert::IsTrue(std::ranges::all_of(layout.windows, [](const Outpost::Hud::Window& _window) { return _window.frame.color.w == 1.0f; }),
                   L"opaque");

    // Research's page, first, in the middle and last.
    Assert::IsTrue(
      std::ranges::any_of(layout.texts, [](const Outpost::Hud::Text& _text) { return _text.text == "TOPICS \xC2\xB7 1-10 OF 21"; }));
    const auto pageOf = [&content](std::size_t _first)
    {
      Outpost::Hud::Content paged = content;
      if (paged.laboratory.has_value())
        paged.laboratory->firstTopic = _first;
      const Outpost::Hud::Layout pagedLayout = Lay(paged, 1920, 1080);
      const auto page =
        std::ranges::find_if(pagedLayout.texts, [](const Outpost::Hud::Text& _text) { return _text.text.starts_with("TOPICS"); });
      return page != pagedLayout.texts.end() ? page->text : std::string();
    };
    Assert::AreEqual(std::string("TOPICS \xC2\xB7 11-20 OF 21"), pageOf(10));
    Assert::AreEqual(std::string("TOPICS \xC2\xB7 13-21 OF 21"), pageOf(Outpost::Hud::StepTopics(20, 0, 21)));
  }

  // ADR-068 (task 16.2): an ore asteroid's mark is an outlined square, so that it differs from an enemy's filled one in
  // shape; a field stands at 2:1 or more against the map, and a dry asteroid at 3:1, computed as the text's contrast is.
  TEST_METHOD(SetsTheMinimapsMarksApart)
  {
    Outpost::Hud::Content content{.ore = 0, .selection = {}, .mapSizeMeters = 2000.0f};
    const Outpost::PlanePosition ore{.xMeters = -600.0f};
    const Outpost::PlanePosition field{.xMeters = 0.0f};
    const Outpost::PlanePosition dry{.xMeters = 600.0f};
    const Outpost::PlanePosition enemy{.zMeters = 600.0f};
    content.marks = {
      {.position = ore, .radiusMeters = 45.0f, .side = Outpost::Hud::Side::Neutral, .kind = Outpost::EntityKind::Asteroid},
      {.position = field, .radiusMeters = 150.0f, .side = Outpost::Hud::Side::Neutral, .kind = Outpost::EntityKind::AsteroidField},
      {.position = dry, .radiusMeters = 45.0f, .side = Outpost::Hud::Side::Neutral, .kind = Outpost::EntityKind::Asteroid, .dry = true},
      {.position = enemy, .radiusMeters = 45.0f, .side = Outpost::Hud::Side::Enemy, .kind = Outpost::EntityKind::Structure}};
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const auto over = [&layout](Outpost::PlanePosition _position)
    {
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(_position);
      return std::ranges::count_if(layout.panels,
                                   [&at](const Outpost::Hud::Rect& _rect) { return _rect.Contains(at.x, at.y) && _rect.width < 100.0f; });
    };
    Assert::AreEqual(std::ptrdiff_t{0}, over(ore), L"an ore asteroid's middle is open");
    Assert::AreEqual(std::ptrdiff_t{0}, over(dry));
    Assert::AreEqual(std::ptrdiff_t{1}, over(enemy), L"an enemy's mark is filled");

    // The ground under the marks: the minimap's frame and the map over it, as the text's ground is computed.
    const DirectX::XMFLOAT2 corner{layout.minimap.left + 1.0f, layout.minimap.top + 1.0f};
    const std::vector<DirectX::XMFLOAT3> grounds = GroundsOf(layout, 0, Outpost::Hud::Text{.text = "x", .left = corner.x, .top = corner.y});
    Assert::AreEqual(size_t{1}, grounds.size());
    const auto contrastAt = [&](Outpost::PlanePosition _position, float _dx)
    {
      const DirectX::XMFLOAT2 at = layout.MinimapPixelOf(_position);
      const auto mark = std::ranges::find_if(layout.panels, [&](const Outpost::Hud::Rect& _rect)
                                             { return _rect.Contains(at.x + _dx, at.y) && _rect.width < 100.0f; });
      Assert::IsTrue(mark != layout.panels.end());
      return ContrastOf({mark->color.x, mark->color.y, mark->color.z}, grounds.front());
    };
    const DirectX::XMFLOAT2 oreAt = layout.MinimapPixelOf(ore);
    const auto oreEdge = std::ranges::find_if(layout.panels, [&](const Outpost::Hud::Rect& _rect)
                                              { return _rect.Contains(oreAt.x, oreAt.y - 5.5f) && _rect.width < 100.0f; });
    Assert::IsTrue(oreEdge != layout.panels.end(), L"the outline's top");
    Assert::IsTrue(contrastAt(field, 0.0f) >= 2.0f, L"a field at 2:1");
    const DirectX::XMFLOAT2 dryAt = layout.MinimapPixelOf(dry);
    const auto dryEdge = std::ranges::find_if(layout.panels, [&](const Outpost::Hud::Rect& _rect)
                                              { return _rect.Contains(dryAt.x, dryAt.y - 5.5f) && _rect.width < 100.0f; });
    Assert::IsTrue(dryEdge != layout.panels.end());
    Assert::IsTrue(ContrastOf({dryEdge->color.x, dryEdge->color.y, dryEdge->color.z}, grounds.front()) >= 3.0f, L"a dry asteroid at 3:1");
    Assert::IsTrue(LuminanceOf({dryEdge->color.x, dryEdge->color.y, dryEdge->color.z}) <
                     LuminanceOf({oreEdge->color.x, oreEdge->color.y, oreEdge->color.z}),
                   L"a dry asteroid darker than one with ore");
  }

  // ADR-068 (task 16.3): a Shipyard's card says how long its design takes and how it does against each hull, three
  // segments a hull lit by the designer's rating, three for Good, two for Fair and one for Poor; the Constructor's card says
  // how long it takes.
  TEST_METHOD(SaysWhatAProductionCardBuilds)
  {
    Outpost::Snapshot newest = DesignerSnapshot();
    newest.designs.push_back({.id = LINE,
                              .nameUtf8 = "Lancer",
                              .hull = Outpost::HullId{1},
                              .drive = Outpost::DriveId{1},
                              .weapon = Outpost::WeaponId{2},
                              .cost = 137});
    const Outpost::Hud::ProductionPanel panel = Outpost::Hud::DescribeProduction(newest, &newest.entities.front());
    Assert::AreEqual(size_t{2}, panel.options.size());
    using Rating = Outpost::Hud::Rating;
    const Outpost::Hud::QueueOption& swarm = panel.options[0];
    Assert::AreEqual(std::string("8 s"), swarm.time, L"the designer's build time");
    Assert::AreEqual(size_t{3}, swarm.strengths.size());
    Assert::AreEqual(std::string("S"), swarm.strengths[0].hull);
    Assert::AreEqual(std::string("L"), swarm.strengths[2].hull);
    Assert::IsTrue(swarm.strengths[0].rating == Rating::Fair && swarm.strengths[1].rating == Rating::Fair &&
                   swarm.strengths[2].rating == Rating::Poor);
    // The Lance does the most to a formation of Medium or Large hulls; the Missile Rack's splash does more to Small ones.
    const Outpost::Hud::QueueOption& lancer = panel.options[1];
    Assert::IsTrue(lancer.strengths[0].rating == Rating::Fair && lancer.strengths[1].rating == Rating::Good &&
                   lancer.strengths[2].rating == Rating::Good);

    // Laid out: the time, each hull's initial, and its lit segments.
    Outpost::Hud::Content content;
    content.production = panel;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    const auto card = std::ranges::find_if(layout.actions, [](const auto& _entry) { return _entry.second.design == SWARM; });
    Assert::IsTrue(card != layout.actions.end());
    const Outpost::Hud::Rect area = card->first;
    const auto inCard = [&area](float _x, float _y) { return area.Contains(_x, _y); };
    Assert::IsTrue(std::ranges::any_of(layout.texts, [&](const Outpost::Hud::Text& _text)
                                       { return _text.text == "8 s" && inCard(_text.left, _text.top); }));
    for (const std::string_view hull : {"S", "M", "L"})
      Assert::IsTrue(std::ranges::any_of(layout.texts, [&](const Outpost::Hud::Text& _text)
                                         { return _text.text == hull && inCard(_text.left, _text.top); }));
    std::size_t segments = 0;
    std::size_t lit = 0;
    for (const Outpost::Hud::Rect& panelRect : layout.panels)
    {
      if (panelRect.width == 6.0f && panelRect.height == 8.0f && inCard(panelRect.left, panelRect.top))
      {
        ++segments;
        lit += panelRect.color.x + panelRect.color.y > 0.1f ? 1 : 0;
      }
    }
    Assert::AreEqual(size_t{9}, segments);
    Assert::AreEqual(size_t{5}, lit, L"two, two and one");

    // The Constructor's card.
    newest.constructorBuildSeconds = 15.0;
    const Outpost::EntityView station{.id = Outpost::EntityId{20},
                                      .kind = Outpost::EntityKind::Structure,
                                      .owner = PLAYER,
                                      .structure = Outpost::StructureKind::CommandStation};
    const Outpost::Hud::ProductionPanel constructors = Outpost::Hud::DescribeProduction(newest, &station);
    Assert::AreEqual(std::string("15 s"), constructors.options[0].time);
    Assert::IsTrue(constructors.options[0].strengths.empty());
  }

  // ADR-068 (task 16.4): F1's Controls window lists every key the game reads, each from KeyBindings.h, where the input code
  // reads it, so that the two cannot disagree; and it opens in the middle of the screen.
  TEST_METHOD(ListsEveryKeyInTheControlsWindow)
  {
    Outpost::Hud::Content content;
    content.controls = true;
    const Outpost::Hud::Layout layout = Lay(content, 1920, 1080);
    Assert::AreEqual(size_t{1}, layout.windows.size());
    const Outpost::Hud::Window& window = layout.windows.front();
    Assert::IsTrue(window.kind == Outpost::WindowKind::Controls);
    Assert::AreEqual(960.0f, window.frame.left + (window.frame.width / 2.0f), 0.5f, L"in the middle");
    Assert::IsTrue(window.frame.top >= 0.0f && window.frame.top + window.frame.height <= 1080.0f);

    // The keys' column: every text that starts at the window's inset, under its title bar.
    std::vector<std::string> keys;
    const Outpost::Hud::Span texts = layout.TextsOf(1);
    for (std::size_t t = texts.first; t < texts.end; ++t)
    {
      const Outpost::Hud::Text& line = layout.texts[t];
      if (std::abs(line.left - (window.frame.left + 24.0f)) < 0.5f && line.top > window.titleBar.top + window.titleBar.height)
        keys.push_back(line.text);
    }
    // A key's name stands as a word of its own in some line of the column.
    const auto named = [&keys](const std::string& _name)
    {
      const auto isWordCharacter = [](char _character) { return std::isalnum(static_cast<unsigned char>(_character)) != 0; };
      return std::ranges::any_of(keys,
                                 [&](const std::string& _line)
                                 {
                                   for (std::size_t at = _line.find(_name); at != std::string::npos; at = _line.find(_name, at + 1))
                                   {
                                     const bool start = at == 0 || !isWordCharacter(_line[at - 1]);
                                     const bool end = at + _name.size() == _line.size() || !isWordCharacter(_line[at + _name.size()]);
                                     if (start && end)
                                       return true;
                                   }
                                   return false;
                                 });
    };
    for (const std::uint8_t key : {Outpost::KEY_ATTACK_MOVE,
                                   Outpost::KEY_STOP,
                                   Outpost::KEY_HOLD_SECTOR,
                                   Outpost::KEY_PATROL,
                                   Outpost::KEY_FIRST_GROUP,
                                   Outpost::KEY_LAST_GROUP,
                                   Outpost::KEY_PAN_UP,
                                   Outpost::KEY_PAN_LEFT,
                                   Outpost::KEY_PAN_DOWN,
                                   Outpost::KEY_PAN_RIGHT,
                                   Outpost::KEY_TURN_COUNTERCLOCKWISE,
                                   Outpost::KEY_TURN_CLOCKWISE,
                                   Outpost::KEY_DESIGNER,
                                   Outpost::KEY_PRODUCTION,
                                   Outpost::KEY_RESEARCH,
                                   Outpost::KEY_CONTROLS,
                                   Outpost::KEY_CANCEL,
                                   Outpost::KEY_LATEST_ALERT,
                                   Outpost::KEY_EVERY_HEALTH_BAR,
                                   Outpost::KEY_LARGER_INTERFACE,
                                   Outpost::KEY_SMALLER_INTERFACE})
    {
      Assert::IsTrue(named(Outpost::KeyName(key)), std::wstring(winrt::to_hstring(Outpost::KeyName(key))).c_str());
    }
    Assert::AreEqual(std::string("F1"), Outpost::KeyName(Outpost::KEY_CONTROLS));
    Assert::AreEqual(Outpost::KeyBindings().size(), keys.size(), L"a line in the column for each binding");
  }

  // ADR-070 (task 17.1): Ctrl+= and Ctrl+- step the interface's own scale through its steps, a factor on the screen's, as
  // far as the designer, laid out for the match's components, and the Controls window fit the screen between its margins;
  // a new screen or match takes it down to the largest step that fits.
  TEST_METHOD(StepsTheInterfaceAsFarAsItsWindowsFit)
  {
    using Hud = Outpost::Hud;
    Assert::AreEqual(1.25f, Hud::Scale(1920, 1080, 1.25f), 1e-6f);
    Assert::AreEqual(2.5f, Hud::Scale(3840, 2160, 1.25f), 1e-6f);

    // The menu has no designer: the Controls window, about 740 units tall, fits a 1080-unit screen up to 125%.
    Assert::AreEqual(1.1f, Hud::StepInterface(1.0f, 1, nullptr, 1920, 1080));
    Assert::AreEqual(1.25f, Hud::StepInterface(1.1f, 1, nullptr, 1920, 1080));
    Assert::AreEqual(1.25f, Hud::StepInterface(1.25f, 1, nullptr, 1920, 1080), L"150% leaves the Controls window no room");

    // With five weapons and the module row, the designer is 913 units tall: 110% fits 16:9, and 125% a 3:2 screen.
    const Outpost::Snapshot newest = DesignerSnapshot(true, true);
    Assert::IsTrue(Hud::WindowsFit(&newest, 1920, 1080, 1.1f));
    Assert::IsFalse(Hud::WindowsFit(&newest, 1920, 1080, 1.25f));
    Assert::AreEqual(1.1f, Hud::StepInterface(1.0f, 1, &newest, 1920, 1080));
    Assert::AreEqual(1.1f, Hud::StepInterface(1.1f, 1, &newest, 1920, 1080), L"no further than fits");
    Assert::AreEqual(1.1f, Hud::StepInterface(1.1f, 1, &newest, 3840, 2160), L"the same at any 16:9 size");
    Assert::AreEqual(1.25f, Hud::StepInterface(1.1f, 1, &newest, 2880, 1920));
    Assert::AreEqual(1.25f, Hud::StepInterface(1.25f, 1, &newest, 2880, 1920));

    // Down a step at a time, never below the first; and a match that starts at a step its designer does not fit at comes
    // down to one it does.
    Assert::AreEqual(1.1f, Hud::StepInterface(1.25f, -1, &newest, 2880, 1920));
    Assert::AreEqual(1.0f, Hud::StepInterface(1.0f, -1, &newest, 1920, 1080));
    Assert::AreEqual(1.1f, Hud::StepInterface(1.25f, 0, &newest, 1920, 1080));
    Assert::AreEqual(1.25f, Hud::StepInterface(1.25f, 0, nullptr, 1920, 1080), L"the menu keeps it");
  }

  // ADR-070 (task 17.1): a moved window stays where it can be taken hold of across a step of the interface's own scale, and
  // the layout is set at the step.
  TEST_METHOD(KeepsAMovedWindowOnTheScreenAcrossAStep)
  {
    Outpost::WindowManager windows;
    windows.Open(Outpost::WindowKind::Designer);
    windows.Grab(Outpost::WindowKind::Designer, {}, {.xUnits = 1500.0f, .yUnits = 1000.0f});
    windows.Release();
    for (const float factor : Outpost::Hud::INTERFACE_STEPS)
    {
      const Outpost::Hud::Layout layout = Lay(WithDesigner(), 1920, 1080, {}, &windows, factor);
      Assert::AreEqual(Outpost::Hud::FONT_UNITS * factor, layout.fontPixels, 1e-4f);
      const Outpost::Hud::Window& window = layout.windows.front();
      Assert::IsTrue(window.titleBar.top >= 0.0f && window.titleBar.top + window.titleBar.height <= 1080.01f,
                     L"its title bar on the screen");
      Assert::IsTrue(window.frame.left + (Outpost::Hud::WINDOW_KEPT_ON_SCREEN_UNITS * factor) <= 1920.01f);
      Assert::AreEqual(Outpost::Hud::TITLE_BAR_UNITS * factor, window.titleBar.height, 1e-3f);
    }
  }

  // Task 14.1: at either size, with the longest content, every text ends inside the smallest panel it starts in, measured
  // with the fonts it is drawn in. Every offender is named, not only the first.
  TEST_METHOD(KeepsEveryTextInsideItsPanel)
  {
    std::wstring offenders;
    for (const auto& [width, height] : std::array<std::pair<std::uint32_t, std::uint32_t>, 2>{{{1920, 1080}, {1280, 720}}})
    {
      for (const auto& [name, layout] : LongestLayouts(width, height))
      {
        const std::span<const Neuron::GlyphAtlas::Font> fonts = FontsOf(layout, width, height);
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
      for (const auto& [name, layout] : LongestLayouts(width, height))
      {
        const std::span<const Neuron::GlyphAtlas::Font> fonts = FontsOf(layout, width, height);
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

  // Task 14.2: every text stands at 4.5:1 or more against what it is drawn on, the panels under its first pixel with a
  // hatched panel's stripes and the gaps between them both, as the review computed contrast. K1 exempts no text (ADR-062).
  // Every offender is named, at the least contrast it meets.
  TEST_METHOD(SetsEveryTextAtFourAndAHalfToOne)
  {
    std::wstring offenders;
    for (const auto& [name, layout] : LongestLayouts(1920, 1080))
    {
      for (std::size_t layer = 0; layer < layout.LayerCount(); ++layer)
      {
        const Outpost::Hud::Span texts = layout.TextsOf(layer);
        for (std::size_t t = texts.first; t < texts.end; ++t)
        {
          const Outpost::Hud::Text& text = layout.texts[t];
          if (text.text.empty())
            continue;
          const DirectX::XMFLOAT3 ink{text.color.x, text.color.y, text.color.z};
          float least = std::numeric_limits<float>::max();
          for (const DirectX::XMFLOAT3& ground : GroundsOf(layout, layer, text))
            least = std::min(least, ContrastOf(ink, ground));
          if (least < 4.5f)
            offenders += std::format(L"{} stands at {:.2f}:1\n", Named(name, 1920, text), least);
        }
      }
    }
    Assert::IsTrue(offenders.empty(), offenders.c_str());
  }
};
} // namespace GameAppTests