#include "pch.h"
#include "RepositoryAssets.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId ME{1};
constexpr Outpost::PlayerId ENEMY{2};
constexpr Outpost::Viewport VIEWPORT{.widthPixels = 1920, .heightPixels = 1080};
constexpr Outpost::HullId SMALL{1};
constexpr Outpost::HullId MEDIUM{2};

Outpost::EntityView Ship(std::uint32_t _id, Outpost::PlayerId _owner, float _xMeters, float _zMeters, Outpost::HullId _hull = SMALL)
{
  return {.id = Outpost::EntityId{_id},
          .kind = Outpost::EntityKind::Ship,
          .owner = _owner,
          .hull = _hull,
          .position = {.xMeters = _xMeters, .zMeters = _zMeters},
          .radiusMeters = 8.0f};
}

// Three of mine near the focus, one Medium of mine, and an enemy, all on screen at the default zoom.
std::vector<Outpost::EntityView> World()
{
  return {Ship(1, ME, -60.0f, 0.0f), Ship(2, ME, 0.0f, 0.0f), Ship(3, ME, 60.0f, 0.0f), Ship(4, ME, 0.0f, 80.0f, MEDIUM),
          Ship(5, ENEMY, 120.0f, -60.0f)};
}

// A world far off screen, for what "visible" means.
Outpost::EntityView FarShip(std::uint32_t _id)
{
  return Ship(_id, ME, 900.0f, 900.0f);
}

// Where an entity shows, as a whole pixel, as a click would land.
std::pair<std::int32_t, std::int32_t> PixelOf(const Outpost::Camera& _camera, const Outpost::EntityView& _entity)
{
  const DirectX::XMFLOAT2 pixel = _camera.PixelOf(_entity.position, VIEWPORT).value_or(DirectX::XMFLOAT2{-1.0f, -1.0f});
  return {static_cast<std::int32_t>(std::lround(pixel.x)), static_cast<std::int32_t>(std::lround(pixel.y))};
}

// Drives the controls one frame at a time with scripted events.
class Driver
{
public:
  Driver()
    : m_camera(Outpost::LoadCameraSettings(ReadRepositoryAssetText("Camera.json"))),
      m_world(World())
  {
  }

  void Click(const Outpost::EntityView& _at, std::uint8_t _button = VK_LBUTTON, bool _shift = false)
  {
    const auto [x, y] = PixelOf(m_camera, _at);
    ClickPixel(x, y, _button, _shift);
  }

  void ClickPixel(std::int32_t _x, std::int32_t _y, std::uint8_t _button = VK_LBUTTON, bool _shift = false)
  {
    Frame({Event(Neuron::InputEventKind::ButtonDown, _button, _x, _y, _shift),
           Event(Neuron::InputEventKind::ButtonUp, _button, _x, _y, _shift)},
          _x, _y);
  }

  void Drag(std::int32_t _x0, std::int32_t _y0, std::int32_t _x1, std::int32_t _y1)
  {
    Frame({Event(Neuron::InputEventKind::ButtonDown, VK_LBUTTON, _x0, _y0)}, _x0, _y0);
    Frame({}, _x1, _y1);
    Frame({Event(Neuron::InputEventKind::ButtonUp, VK_LBUTTON, _x1, _y1)}, _x1, _y1);
  }

  void Key(std::uint8_t _key, bool _control = false)
  {
    Neuron::InputEvent event = Event(Neuron::InputEventKind::KeyDown, _key, 0, 0);
    event.control = _control;
    Frame({event}, 0, 0);
  }

  void Wait(std::uint32_t _milliseconds)
  {
    m_clockMilliseconds += _milliseconds;
  }

  std::vector<Outpost::EntityView>& WorldView()
  {
    return m_world;
  }

  Outpost::PlayerControls& Controls()
  {
    return m_controls;
  }

  Outpost::Camera& CameraView()
  {
    return m_camera;
  }

  [[nodiscard]] std::vector<std::uint32_t> Selected() const
  {
    std::vector<std::uint32_t> ids;
    for (const Outpost::EntityId id : m_controls.Selected())
      ids.push_back(id.value);
    return ids;
  }

private:
  Neuron::InputEvent Event(Neuron::InputEventKind _kind, std::uint8_t _key, std::int32_t _x, std::int32_t _y, bool _shift = false)
  {
    m_clockMilliseconds += 10;
    return {.kind = _kind, .key = _key, .xPixels = _x, .yPixels = _y, .timeMilliseconds = m_clockMilliseconds, .shift = _shift};
  }

  void Frame(std::vector<Neuron::InputEvent> _events, std::int32_t _cursorX, std::int32_t _cursorY)
  {
    Neuron::InputState input;
    input.active = true;
    input.cursorXPixels = _cursorX;
    input.cursorYPixels = _cursorY;
    input.events = std::move(_events);
    m_controls.Update(input, m_world, ME, m_camera, VIEWPORT);
  }

  Outpost::Camera m_camera;
  std::vector<Outpost::EntityView> m_world;
  Outpost::PlayerControls m_controls;
  std::uint32_t m_clockMilliseconds = 1000;
};

using Ids = std::vector<std::uint32_t>;

// Task 4.2's world: World(), with a Constructor of mine, a Shipyard of mine under construction, a whole Command Station
// of mine, and an enemy Shipyard.
constexpr std::uint32_t CONSTRUCTOR = 11;
constexpr std::uint32_t SITE = 12;
constexpr std::uint32_t STATION = 13;
constexpr std::uint32_t ENEMY_YARD = 14;

void AddBase(std::vector<Outpost::EntityView>& _world)
{
  Outpost::EntityView constructor = Ship(CONSTRUCTOR, ME, -120.0f, 0.0f);
  constructor.role = Outpost::ShipRole::Constructor;
  constructor.hull = {};
  _world.push_back(constructor);
  const auto structure = [](std::uint32_t _id, Outpost::PlayerId _owner, float _x, float _z, std::int32_t _built)
  {
    return Outpost::EntityView{.id = Outpost::EntityId{_id},
                               .kind = Outpost::EntityKind::Structure,
                               .owner = _owner,
                               .structure = Outpost::StructureKind::Shipyard,
                               .position = {.xMeters = _x, .zMeters = _z},
                               .radiusMeters = 30.0f,
                               .hitPointsHundredths = 100000,
                               .maxHitPointsHundredths = 250000,
                               .builtPermille = _built};
  };
  _world.push_back(structure(SITE, ME, -120.0f, -120.0f, 300));
  Outpost::EntityView station = structure(STATION, ME, 120.0f, 120.0f, Outpost::PERMILLE);
  station.structure = Outpost::StructureKind::CommandStation;
  station.hitPointsHundredths = station.maxHitPointsHundredths;
  _world.push_back(station);
  _world.push_back(structure(ENEMY_YARD, ENEMY, -150.0f, 120.0f, Outpost::PERMILLE));
}

template <typename OrderType> const OrderType* Only(const std::vector<Outpost::Command>& _commands, size_t _index = 0)
{
  Assert::IsTrue(_commands.size() > _index);
  const auto* order = std::get_if<OrderType>(&_commands[_index].order);
  Assert::IsNotNull(order);
  return order;
}
} // namespace

TEST_CLASS(PlayerControlsTests)
{
public:
  TEST_METHOD(PicksTheNearestShipUnderTheCursor)
  {
    Driver driver;
    const Outpost::Camera& camera = driver.CameraView();
    const std::vector<Outpost::EntityView> world = World();
    const auto [x, y] = PixelOf(camera, world[1]);
    const auto mine = [](const Outpost::EntityView& _ship) { return _ship.owner == ME; };
    const std::optional<Outpost::EntityId> picked =
      Outpost::PickShip(world, camera, VIEWPORT, {static_cast<float>(x) + 3.0f, static_cast<float>(y)}, mine);
    Assert::IsTrue(picked == Outpost::EntityId{2});
    // Far from every ship, nothing.
    Assert::IsFalse(Outpost::PickShip(world, camera, VIEWPORT, {5.0f, 5.0f}, mine).has_value());
    // The enemy is not mine.
    const auto [ex, ey] = PixelOf(camera, world[4]);
    Assert::IsFalse(Outpost::PickShip(world, camera, VIEWPORT, {static_cast<float>(ex), static_cast<float>(ey)}, mine).has_value());
  }

  TEST_METHOD(BoxSelectsOnlyMyShipsInTheBox)
  {
    Driver driver;
    const std::vector<Outpost::EntityView> world = World();
    const auto [x1, y1] = PixelOf(driver.CameraView(), world[0]);
    const auto [x3, y3] = PixelOf(driver.CameraView(), world[2]);
    const std::vector<Outpost::EntityId> boxed =
      Outpost::ShipsInBox(world, driver.CameraView(), VIEWPORT,
                          Outpost::ScreenRect::Between(static_cast<float>(x1 - 20), static_cast<float>(y1 - 20),
                                                       static_cast<float>(x3 + 20), static_cast<float>(y3 + 20)),
                          ME);
    Assert::AreEqual(size_t{3}, boxed.size());
  }

  TEST_METHOD(ClickSelectsAndShiftAdds)
  {
    Driver driver;
    driver.Click(driver.WorldView()[0]);
    Assert::IsTrue(Ids{1} == driver.Selected());
    driver.Wait(1000);
    driver.Click(driver.WorldView()[2], VK_LBUTTON, true);
    Assert::IsTrue((Ids{1, 3}) == driver.Selected());
    // Shift-clicking a selected ship takes it out.
    driver.Wait(1000);
    driver.Click(driver.WorldView()[0], VK_LBUTTON, true);
    Assert::IsTrue(Ids{3} == driver.Selected());
    // A click on empty ground clears the selection.
    driver.Wait(1000);
    driver.ClickPixel(5, 5);
    Assert::IsTrue(driver.Selected().empty());
  }

  TEST_METHOD(DragSelectsTheBox)
  {
    Driver driver;
    const auto [x1, y1] = PixelOf(driver.CameraView(), driver.WorldView()[0]);
    const auto [x3, y3] = PixelOf(driver.CameraView(), driver.WorldView()[2]);
    driver.Drag(x1 - 20, y1 - 20, x3 + 20, y3 + 20);
    Assert::IsTrue((Ids{1, 2, 3}) == driver.Selected());
    Assert::IsFalse(driver.Controls().DragBox().has_value());
  }

  TEST_METHOD(DoubleClickSelectsEveryVisibleShipOfThatDesign)
  {
    Driver driver;
    driver.WorldView().push_back(FarShip(6));
    driver.Click(driver.WorldView()[1]);
    driver.Click(driver.WorldView()[1]);
    // The Smalls on screen; not the Medium, not the enemy, not the Small off screen.
    Assert::IsTrue((Ids{1, 2, 3}) == driver.Selected());
  }

  TEST_METHOD(RightClickMovesOrAttacks)
  {
    Driver driver;
    driver.Click(driver.WorldView()[1]);
    const Outpost::Viewport viewport = VIEWPORT;
    driver.ClickPixel(static_cast<std::int32_t>(viewport.widthPixels / 2), 100, VK_RBUTTON);
    std::vector<Outpost::Command> commands = driver.Controls().TakeCommands();
    Assert::AreEqual(size_t{1}, commands.size());
    const auto* move = std::get_if<Outpost::MoveCommand>(&commands[0].order);
    Assert::IsNotNull(move);
    Assert::IsTrue(move->ships == std::vector<Outpost::EntityId>{Outpost::EntityId{2}});
    // Above the middle of the screen is ahead of the focus: +z at the start.
    Assert::IsTrue(move->destination.zMeters > 0.0f);

    driver.Click(driver.WorldView()[4], VK_RBUTTON);
    commands = driver.Controls().TakeCommands();
    const auto* attack = std::get_if<Outpost::AttackCommand>(&commands.at(0).order);
    Assert::IsNotNull(attack);
    Assert::IsTrue(attack->target == Outpost::EntityId{5});
  }

  TEST_METHOD(AThenClickAttackMovesAndSStops)
  {
    Driver driver;
    driver.Click(driver.WorldView()[1]);
    driver.Key('A');
    Assert::IsTrue(driver.Controls().IsAttackMoveArmed());
    driver.ClickPixel(960, 200);
    Assert::IsFalse(driver.Controls().IsAttackMoveArmed());
    // The click was an order, not a selection.
    Assert::IsTrue(Ids{2} == driver.Selected());
    driver.Key('S');
    const std::vector<Outpost::Command> commands = driver.Controls().TakeCommands();
    Assert::AreEqual(size_t{2}, commands.size());
    Assert::IsNotNull(std::get_if<Outpost::AttackMoveCommand>(&commands[0].order));
    Assert::IsNotNull(std::get_if<Outpost::StopCommand>(&commands[1].order));
  }

  // ADR-059: H, then a click, holds the sector the click is in; T, then a click, patrols to it; Escape or a right-click
  // cancels either, and A replaces it.
  TEST_METHOD(HAndTThenClickGiveStandingOrders)
  {
    Driver driver;
    driver.Click(driver.WorldView()[1]);
    driver.Key('H');
    Assert::IsTrue(driver.Controls().ArmedStanding() == Outpost::StandingOrder::HoldSector);
    driver.ClickPixel(960, 200);
    Assert::IsFalse(driver.Controls().ArmedStanding().has_value());
    Assert::IsTrue(Ids{2} == driver.Selected(), L"the click was an order, not a selection");
    driver.Key('T');
    Assert::IsTrue(driver.Controls().ArmedStanding() == Outpost::StandingOrder::Patrol);
    driver.ClickPixel(960, 200);
    std::vector<Outpost::Command> commands = driver.Controls().TakeCommands();
    Assert::AreEqual(size_t{2}, commands.size());
    const auto* hold = std::get_if<Outpost::HoldSectorCommand>(&commands[0].order);
    const auto* patrol = std::get_if<Outpost::PatrolCommand>(&commands[1].order);
    Assert::IsTrue(hold != nullptr && hold->ships == std::vector<Outpost::EntityId>{Outpost::EntityId{2}});
    Assert::IsTrue(patrol != nullptr && patrol->destination == hold->position);

    driver.Key('H');
    driver.Key(VK_ESCAPE);
    Assert::IsFalse(driver.Controls().ArmedStanding().has_value());
    driver.Key('T');
    driver.Key('A');
    Assert::IsFalse(driver.Controls().ArmedStanding().has_value());
    Assert::IsTrue(driver.Controls().IsAttackMoveArmed());
    Assert::IsTrue(driver.Controls().TakeCommands().empty());
  }

  TEST_METHOD(EscapeCancelsAttackMove)
  {
    Driver driver;
    driver.Click(driver.WorldView()[1]);
    driver.Key('A');
    driver.Key(VK_ESCAPE);
    Assert::IsFalse(driver.Controls().IsAttackMoveArmed());
    Assert::IsTrue(driver.Controls().TakeCommands().empty());
  }

  TEST_METHOD(ControlGroupsAssignRecallAndCenter)
  {
    Driver driver;
    driver.Click(driver.WorldView()[0]);
    driver.Click(driver.WorldView()[2], VK_LBUTTON, true);
    driver.Key('1', true);
    driver.Wait(1000);
    driver.ClickPixel(5, 5);
    Assert::IsTrue(driver.Selected().empty());

    driver.Wait(1000);
    driver.Key('1');
    Assert::IsTrue((Ids{1, 3}) == driver.Selected());
    // A single tap leaves the camera; a double tap centers it on the group, between ships 1 and 3.
    driver.CameraView().Pan(300.0f, 300.0f);
    driver.Wait(1000);
    driver.Key('1');
    Assert::AreEqual(300.0f, driver.CameraView().Focus().x, 0.01f);
    driver.Key('1');
    Assert::AreEqual(0.0f, driver.CameraView().Focus().x, 0.01f);
    Assert::AreEqual(0.0f, driver.CameraView().Focus().y, 0.01f);
  }

  // Task 4.5: a click on one of my structures selects it alone, and a box takes ships only.
  TEST_METHOD(SelectsOneOfMyStructures)
  {
    Driver driver;
    AddBase(driver.WorldView());
    driver.Click(driver.WorldView()[0]);
    driver.Click(driver.WorldView()[7]);
    Assert::IsTrue((Ids{STATION}) == driver.Selected());
    driver.Click(driver.WorldView()[8]);
    Assert::IsTrue(driver.Selected().empty(), L"an enemy structure is not selected");
    driver.Drag(0, 0, 1919, 1079);
    const Ids boxed = driver.Selected();
    Assert::IsTrue(std::ranges::find(boxed, STATION) == boxed.end());
  }

  // Task 4.2: selected Constructors right-clicked on my own structure under construction, or damaged, work on it, and
  // the warships with them go there; a whole one is only a destination.
  TEST_METHOD(ConstructorsRepairAFriendlyThatNeedsIt)
  {
    Driver driver;
    AddBase(driver.WorldView());
    driver.Click(driver.WorldView()[5]);
    driver.Click(driver.WorldView()[0], VK_LBUTTON, true);
    Assert::IsTrue((Ids{1, CONSTRUCTOR}) == driver.Selected());
    (void)driver.Controls().TakeCommands();

    driver.Click(driver.WorldView()[6], VK_RBUTTON);
    std::vector<Outpost::Command> commands = driver.Controls().TakeCommands();
    Assert::AreEqual(size_t{2}, commands.size());
    const auto* repair = Only<Outpost::RepairCommand>(commands);
    Assert::IsTrue(repair->target == Outpost::EntityId{SITE});
    Assert::IsTrue(repair->constructors == std::vector{Outpost::EntityId{CONSTRUCTOR}});
    const auto* move = Only<Outpost::MoveCommand>(commands, 1);
    Assert::IsTrue(move->ships == std::vector{Outpost::EntityId{1}});

    driver.Click(driver.WorldView()[7], VK_RBUTTON);
    commands = driver.Controls().TakeCommands();
    Assert::AreEqual(size_t{1}, commands.size());
    Assert::AreEqual(size_t{2}, Only<Outpost::MoveCommand>(commands)->ships.size(), L"a whole structure is a destination");
  }

  TEST_METHOD(RightClickAttacksAnEnemyStructure)
  {
    Driver driver;
    AddBase(driver.WorldView());
    driver.Click(driver.WorldView()[0]);
    (void)driver.Controls().TakeCommands();
    driver.Click(driver.WorldView()[8], VK_RBUTTON);
    const std::vector<Outpost::Command> commands = driver.Controls().TakeCommands();
    Assert::IsTrue(Only<Outpost::AttackCommand>(commands)->target == Outpost::EntityId{ENEMY_YARD});
  }

  // Task 4.2: the HUD arms a placement for selected Constructors; the next left-click on the ground orders the build,
  // Shift keeps it armed, and right-click or Escape cancels it.
  TEST_METHOD(PlacesAStructureWithTheNextLeftClick)
  {
    Driver driver;
    AddBase(driver.WorldView());
    driver.Click(driver.WorldView()[0]);
    driver.Controls().ArmPlacement(Outpost::StructureKind::DefensePlatform, driver.WorldView());
    Assert::IsFalse(driver.Controls().Placing().has_value(), L"no Constructor is selected");

    driver.Click(driver.WorldView()[5]);
    driver.Controls().ArmPlacement(Outpost::StructureKind::DefensePlatform, driver.WorldView());
    Assert::IsTrue(driver.Controls().Placing() == Outpost::StructureKind::DefensePlatform);
    (void)driver.Controls().TakeCommands();
    driver.ClickPixel(800, 400, VK_LBUTTON, true);
    Assert::IsTrue(driver.Controls().Placing().has_value(), L"Shift keeps it armed");
    driver.ClickPixel(900, 400);
    Assert::IsFalse(driver.Controls().Placing().has_value());
    const std::vector<Outpost::Command> commands = driver.Controls().TakeCommands();
    Assert::AreEqual(size_t{2}, commands.size());
    const auto* build = Only<Outpost::BuildStructureCommand>(commands, 1);
    Assert::IsTrue(build->structure == Outpost::StructureKind::DefensePlatform);
    Assert::IsTrue(build->constructors == std::vector{Outpost::EntityId{CONSTRUCTOR}});
    const Outpost::PlanePosition ground =
      driver.CameraView().GroundPointAtPixel(900.0f, 400.0f, VIEWPORT).value_or(Outpost::PlanePosition{});
    Assert::AreEqual(ground.xMeters, build->position.xMeters, 0.01f);
    Assert::AreEqual(ground.zMeters, build->position.zMeters, 0.01f);
    Assert::IsTrue(Ids{CONSTRUCTOR} == driver.Selected(), L"placing does not change the selection");

    driver.Controls().ArmPlacement(Outpost::StructureKind::Shipyard, driver.WorldView());
    driver.ClickPixel(900, 400, VK_RBUTTON);
    Assert::IsFalse(driver.Controls().Placing().has_value());
    Assert::IsTrue(driver.Controls().TakeCommands().empty(), L"the right-click only cancels");
    driver.Controls().ArmPlacement(Outpost::StructureKind::Shipyard, driver.WorldView());
    driver.Key(VK_ESCAPE);
    Assert::IsFalse(driver.Controls().Placing().has_value());
  }

  // ADR-046: a Mining Rig is ordered only by an asteroid the controls are given, which leaves out those in space the
  // player has never seen. A click by none orders nothing and leaves the placement armed.
  TEST_METHOD(OrdersAMiningRigOnlyByAKnownAsteroid)
  {
    Driver driver;
    AddBase(driver.WorldView());
    const Outpost::EntityView asteroid{.id = Outpost::EntityId{90},
                                       .kind = Outpost::EntityKind::Asteroid,
                                       .position = {.xMeters = 150.0f, .zMeters = -40.0f},
                                       .radiusMeters = 45.0f};
    driver.Click(driver.WorldView()[5]);
    driver.Controls().ArmPlacement(Outpost::StructureKind::MiningRig, driver.WorldView());
    (void)driver.Controls().TakeCommands();
    driver.Click(asteroid);
    Assert::IsTrue(driver.Controls().TakeCommands().empty(), L"an asteroid the controls are not given");
    Assert::IsTrue(driver.Controls().Placing() == Outpost::StructureKind::MiningRig, L"still armed");

    driver.WorldView().push_back(asteroid);
    driver.Click(asteroid);
    const std::vector<Outpost::Command> commands = driver.Controls().TakeCommands();
    Assert::IsTrue(Only<Outpost::BuildStructureCommand>(commands)->structure == Outpost::StructureKind::MiningRig);
    Assert::IsFalse(driver.Controls().Placing().has_value());
  }

  TEST_METHOD(QueuesAJobFromTheHud)
  {
    Driver driver;
    driver.Controls().Queue(Outpost::EntityId{STATION}, {});
    const std::vector<Outpost::Command> commands = driver.Controls().TakeCommands();
    Assert::IsTrue(Only<Outpost::QueueShipCommand>(commands)->producer == Outpost::EntityId{STATION});
  }

  TEST_METHOD(ForgetsShipsThatAreGone)
  {
    Driver driver;
    driver.Click(driver.WorldView()[0]);
    driver.Key('2', true);
    driver.WorldView().erase(driver.WorldView().begin());
    driver.Key('2');
    Assert::IsTrue(driver.Selected().empty());
  }
};
} // namespace GameAppTests