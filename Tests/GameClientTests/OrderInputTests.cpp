#include "pch.h"

#include "OrderInput.h"

#include "Device.h"
#include "FixedPoint.h"

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Design/Interface.md §6's table as cases, and §7's hotkeys beside it.

namespace ReplicaTests
{

namespace
{

/// A camera looking STRAIGHT DOWN from y = 100, which is what an order needs and what the picking
/// suite's side-on fixture cannot give: a ray with no vertical component never meets the ground, so
/// every Move would come back with nowhere to go.
///
/// world (x, y, z) -> clip (x/100, z/100, y/200 + 1/2), depth reversed (ADR-005). Screen x is world
/// x plus 100; screen y is 100 MINUS world z, because the screen counts downward and +z is away.
[[nodiscard]] Outpost::PickCamera TopDownCamera()
{
  Outpost::PickCamera camera{};
  camera.frameWidth = 200;
  camera.frameHeight = 200;
  camera.viewProjection.m = {0.01f, 0.0f,  0.0f,   0.0f, //
                             0.0f,  0.0f,  0.005f, 0.0f, //
                             0.0f,  0.01f, 0.0f,   0.0f, //
                             0.0f,  0.0f,  0.5f,   1.0f};
  camera.inverseViewProjection.m = {100.0f, 0.0f,    0.0f,   0.0f, //
                                    0.0f,   0.0f,    100.0f, 0.0f, //
                                    0.0f,   200.0f,  0.0f,   0.0f, //
                                    0.0f,   -100.0f, 0.0f,   1.0f};
  return camera;
}

[[nodiscard]] Outpost::PickCandidate Thing(std::uint32_t _id, std::uint8_t _seat, Outpost::ObjectKind _kind, float _x, float _z)
{
  Outpost::PickCandidate candidate{};
  candidate.id = _id;
  candidate.seat = _seat;
  candidate.kind = _kind;
  candidate.x = _x;
  candidate.y = 0.0f;
  candidate.z = _z;
  candidate.radius = 6.0f;
  return candidate;
}

[[nodiscard]] Outpost::SelectedObject Armed(std::uint32_t _id)
{
  return {_id, Outpost::ObjectKind::Device, true, false};
}

[[nodiscard]] Outpost::SelectedObject Unarmed(std::uint32_t _id)
{
  return {_id, Outpost::ObjectKind::Device, false, false};
}

[[nodiscard]] Outpost::SelectedObject Building(std::uint32_t _id)
{
  return {_id, Outpost::ObjectKind::Structure, false, false};
}

/// Subunits from world units, which is what every positional operand of GameShared/Order.h is in.
[[nodiscard]] std::int32_t Sub(float _worldUnits)
{
  return static_cast<std::int32_t>(std::lround(_worldUnits * static_cast<float>(Neuron::SUBUNITS_PER_WORLD_UNIT)));
}

/// What a screen column names on the ground, in subunits, under this camera.
///
/// THE HALF PIXEL IS REAL AND IS NOT ABSORBED INTO A TOLERANCE. GameClient/Picking.cpp casts through
/// the pixel's CENTRE, because that is where the rasterizer sampled what was drawn there, so screen
/// column 150 is world x 50.5 and not 50. The first writing of these cases expected the round
/// number and four of them failed; a tolerance would have hidden it, and half a world unit is a
/// real distance - a test loose enough to swallow it is loose enough to swallow a whole one.
[[nodiscard]] std::int32_t SubAtColumn(std::int32_t _screenX)
{
  return Sub(static_cast<float>(_screenX) + 0.5f - 100.0f);
}

/// And what a screen ROW names: the screen counts downward and +z is away, so row 100 is z = 0 and
/// a row above it is a larger z, again half a pixel over.
[[nodiscard]] std::int32_t SubAtRow(std::int32_t _screenY)
{
  return Sub(100.0f - (static_cast<float>(_screenY) + 0.5f));
}

struct Session
{
  Outpost::OrderInput input;
  std::vector<Outpost::PickCandidate> candidates;
  std::vector<Outpost::PickBox> blocked;
  std::vector<Outpost::SelectedObject> selected;
  std::vector<std::int8_t> keyEdges = std::vector<std::int8_t>(256, 0);
  std::uint32_t structureRowCount = 3;
  bool groundKnown = false;
  std::int32_t groundX = 0;
  std::int32_t groundZ = 0;
  std::uint8_t seat = 0;

  [[nodiscard]] Outpost::OrderFrame Frame(std::int32_t _x, std::int32_t _y, bool _left, bool _right) const
  {
    Outpost::OrderFrame frame{};
    frame.camera = TopDownCamera();
    frame.candidates = candidates;
    frame.blocked = blocked;
    frame.selected = selected;
    frame.ownSeat = seat;
    frame.x = _x;
    frame.y = _y;
    frame.leftPressed = _left;
    frame.rightPressed = _right;
    frame.keyEdges = keyEdges;
    frame.structureRowCount = structureRowCount;
    frame.groundKnown = groundKnown;
    frame.groundX = groundX;
    frame.groundZ = groundZ;
    return frame;
  }

  [[nodiscard]] std::vector<Outpost::Order> RightClick(std::int32_t _x, std::int32_t _y)
  {
    std::vector<Outpost::Order> orders;
    input.Advance(Frame(_x, _y, false, true), orders);
    return orders;
  }

  [[nodiscard]] std::vector<Outpost::Order> LeftClick(std::int32_t _x, std::int32_t _y)
  {
    std::vector<Outpost::Order> orders;
    input.Advance(Frame(_x, _y, true, false), orders);
    return orders;
  }

  [[nodiscard]] std::vector<Outpost::Order> Press(std::uint8_t _key)
  {
    std::vector<Outpost::Order> orders;
    keyEdges[_key] = 1;
    input.Advance(Frame(100, 100, false, false), orders);
    keyEdges[_key] = 0;
    return orders;
  }
};

/// A content tree with four module rows, which is everything AbilitiesOf reads: a gun, a builder, a
/// sensor, and a builder row that carries no build power at all - a legal row, and the one a test
/// of "carries build power" has to have or it is testing the enum and not the ability.
[[nodiscard]] Outpost::ContentTree AbilityTables()
{
  Outpost::ContentTree tree{};
  Outpost::ModuleDesc gun{};
  gun.id = "Gun";
  gun.systemKind = Outpost::SystemKind::None;
  Outpost::ModuleDesc builder{};
  builder.id = "Builder";
  builder.systemKind = Outpost::SystemKind::Builder;
  builder.buildPowerHundredthsPerTick = 50;
  Outpost::ModuleDesc sensor{};
  sensor.id = "Sensor";
  sensor.systemKind = Outpost::SystemKind::Sensor;
  Outpost::ModuleDesc idleBuilder{};
  idleBuilder.id = "IdleBuilder";
  idleBuilder.systemKind = Outpost::SystemKind::Builder;
  idleBuilder.buildPowerHundredthsPerTick = 0;
  tree.components.modules = {gun, builder, sensor, idleBuilder};
  return tree;
}

inline constexpr std::uint32_t GUN_ROW = 0;
inline constexpr std::uint32_t BUILDER_ROW = 1;
inline constexpr std::uint32_t SENSOR_ROW = 2;
inline constexpr std::uint32_t IDLE_BUILDER_ROW = 3;

/// A seat's design of the rows named, at an index, so that a device can point at it.
[[nodiscard]] Outpost::DesignState Design(std::uint8_t _seat, std::uint32_t _index, std::initializer_list<std::uint32_t> _rows)
{
  Outpost::DesignState design{};
  design.seat = _seat;
  design.index = _index;
  design.moduleCount = static_cast<std::uint8_t>(_rows.size());
  std::uint8_t at = 0;
  for (const std::uint32_t row : _rows)
  {
    design.modules[at] = row;
    ++at;
  }
  return design;
}

[[nodiscard]] Outpost::ReplicaDevice DeviceOf(std::uint32_t _id, std::uint8_t _seat, std::uint32_t _design)
{
  Outpost::ReplicaDevice device{};
  device.state.id = _id;
  device.state.seat = _seat;
  device.state.design = _design;
  return device;
}

} // namespace

TEST_CLASS(OrderInputTests)
{
public:
  /// §6 row 1: open ground, devices, Move to that point.
  TEST_METHOD(ARightClickOnOpenGroundMovesEverySelectedDevice)
  {
    Session session;
    session.selected = {Unarmed(1), Unarmed(2)};
    const std::vector<Outpost::Order> orders = session.RightClick(120, 100);
    Assert::AreEqual(std::size_t{2}, orders.size(), L"one order an object, which is the wire's shape");
    for (const Outpost::Order& order : orders)
    {
      Assert::IsTrue(order.kind == Outpost::OrderKind::Move, L"a Move");
      Assert::AreEqual(SubAtColumn(120), order.operands[1], L"x in subunits");
      Assert::AreEqual(SubAtRow(100), order.operands[2], L"z in subunits");
      Assert::AreEqual(std::uint8_t{0}, order.seat, L"and it is this commander's");
    }
  }

  /// §6 rows 2 and 3: an armed device attacks a visible enemy and an unarmed one moves to it.
  TEST_METHOD(ARightClickOnAnEnemySplitsTheSelectionByWhetherItCanShoot)
  {
    Session session;
    session.candidates = {Thing(9, 1, Outpost::ObjectKind::Device, 0.0f, 0.0f)};
    session.selected = {Armed(1), Unarmed(2)};
    const std::vector<Outpost::Order> orders = session.RightClick(100, 100);
    Assert::AreEqual(std::size_t{2}, orders.size(), L"both are ordered, differently");

    Assert::IsTrue(orders[0].kind == Outpost::OrderKind::Attack, L"the armed one attacks");
    Assert::AreEqual(std::int32_t{1}, orders[0].operands[0], L"it is the attacker");
    Assert::AreEqual(std::int32_t{9}, orders[0].operands[1], L"and the enemy is the target");
    Assert::AreEqual(static_cast<std::int32_t>(Outpost::ObjectKind::Device), orders[0].operands[2], L"named by kind too");

    Assert::IsTrue(orders[1].kind == Outpost::OrderKind::Move, L"the unarmed one moves to it");
    Assert::AreEqual(std::int32_t{2}, orders[1].operands[0], L"it is the mover");
  }

  /// §6 row 6: "a structure is selected - nothing; a structure takes no primary order". It is
  /// skipped rather than refusing the whole click, because a commander with a factory and four
  /// trucks selected who right-clicks means the trucks.
  TEST_METHOD(AStructureInTheSelectionTakesNoPrimaryOrder)
  {
    Session session;
    session.selected = {Building(5), Unarmed(6)};
    const std::vector<Outpost::Order> orders = session.RightClick(120, 100);
    Assert::AreEqual(std::size_t{1}, orders.size(), L"only the device is ordered");
    Assert::AreEqual(std::int32_t{6}, orders[0].operands[0], L"and it is the device");
  }

  /// A camera aimed above the horizon has no ground under the cursor, and an order with nowhere to
  /// go is not an order.
  TEST_METHOD(ARightClickThatMeetsNoGroundOrdersNothing)
  {
    Session session;
    session.selected = {Unarmed(1)};
    // A ray parallel to the ground: the side-on camera of the picking suite, which never meets y=0.
    Outpost::OrderFrame frame = session.Frame(100, 100, false, true);
    frame.camera.inverseViewProjection.m = {100.0f, 0.0f, 0.0f,   0.0f, 0.0f, 100.0f, 0.0f,    0.0f,
                                            0.0f,   0.0f, 200.0f, 0.0f, 0.0f, 50.0f,  -100.0f, 1.0f};
    std::vector<Outpost::Order> orders;
    session.input.Advance(frame, orders);
    Assert::AreEqual(std::size_t{0}, orders.size(), L"nothing is ordered at the sky");
  }

  /// A ray aimed ABOVE the horizon meets the ground behind the camera, at a negative distance along
  /// itself, and an order issued there sends the commander's devices to a point he is looking away
  /// from. Mutation testing found this untested: removing the guard changed no result, because the
  /// case above only covers a ray PARALLEL to the ground, which a different guard catches.
  TEST_METHOD(ARightClickAboveTheHorizonOrdersNothing)
  {
    Session session;
    session.selected = {Unarmed(1)};
    Outpost::OrderFrame frame = session.Frame(100, 100, false, true);
    // A camera above the ground looking UP: near at y = 10, far at y = 210, so the ray climbs and
    // the plane y = 0 is behind it.
    frame.camera.inverseViewProjection.m = {100.0f, 0.0f,    0.0f,   0.0f, //
                                            0.0f,   0.0f,    100.0f, 0.0f, //
                                            0.0f,   -200.0f, 0.0f,   0.0f, //
                                            0.0f,   210.0f,  0.0f,   1.0f};
    const Outpost::PickRay ray = Outpost::RayThrough(frame.camera, 100, 100);
    Assert::IsTrue(ray.directionY > 0.0f, L"the ray climbs, or this case tests the parallel guard again");
    std::int32_t x = 0;
    std::int32_t z = 0;
    Assert::IsFalse(Outpost::GroundPoint(ray, x, z), L"and it never meets the ground in front of the camera");

    std::vector<Outpost::Order> orders;
    session.input.Advance(frame, orders);
    Assert::AreEqual(std::size_t{0}, orders.size(), L"so nothing is ordered");
  }

  /// §7: S is Stop, and it needs no click - it is the one order a commander gives when what he
  /// wants is for something to stop happening, and asking him to aim it would be absurd.
  TEST_METHOD(StopIsImmediateAndNeedsNoClick)
  {
    Session session;
    session.selected = {Unarmed(1), Unarmed(2), Building(3)};
    const std::vector<Outpost::Order> orders = session.Press('S');
    Assert::AreEqual(std::size_t{2}, orders.size(), L"both devices, not the structure");
    Assert::IsTrue(orders[0].kind == Outpost::OrderKind::Stop, L"a Stop");
  }

  /// §7: H is the hold-position stance, which is SetStance on the movement axis.
  TEST_METHOD(HoldPositionIsSetStanceOnTheMovementAxis)
  {
    Session session;
    session.selected = {Unarmed(1)};
    const std::vector<Outpost::Order> orders = session.Press('H');
    Assert::AreEqual(std::size_t{1}, orders.size(), L"one order");
    Assert::IsTrue(orders[0].kind == Outpost::OrderKind::SetStance, L"a SetStance");
    Assert::AreEqual(static_cast<std::int32_t>(Outpost::StanceAxis::Movement), orders[0].operands[1], L"on the movement axis");
    Assert::AreEqual(static_cast<std::int32_t>(Outpost::MovementStance::HoldPosition), orders[0].operands[2], L"holding position");
  }

  /// §6: an armed order "changes the cursor and makes the next left click on the world issue that
  /// order instead of selecting".
  TEST_METHOD(AttackMoveIsArmedByItsKeyAndIssuedByTheNextLeftClick)
  {
    Session session;
    session.selected = {Armed(1)};
    Assert::AreEqual(std::size_t{0}, session.Press('R').size(), L"arming sends nothing");
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::AttackMove, L"and it is armed");

    const std::vector<Outpost::Order> orders = session.LeftClick(140, 100);
    Assert::AreEqual(std::size_t{1}, orders.size(), L"the click issues it");
    Assert::IsTrue(orders[0].kind == Outpost::OrderKind::AttackMove, L"an AttackMove");
    Assert::AreEqual(SubAtColumn(140), orders[0].operands[1], L"at the point clicked");
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::None, L"and it disarms afterwards");
  }

  /// §6: "Patrol takes two clicks, the second setting the far point." An order with one end of a
  /// patrol in it is not a patrol, so the first click sends nothing.
  TEST_METHOD(PatrolTakesTwoClicksAndTheFirstSendsNothing)
  {
    Session session;
    session.selected = {Armed(1)};
    (void)session.Press('P');
    Assert::AreEqual(std::size_t{0}, session.LeftClick(120, 100).size(), L"the first click is remembered");
    Assert::IsTrue(session.input.AwaitingSecondPoint(), L"and it says so");

    const std::vector<Outpost::Order> orders = session.LeftClick(100, 80);
    Assert::AreEqual(std::size_t{1}, orders.size(), L"the second issues it");
    Assert::IsTrue(orders[0].kind == Outpost::OrderKind::Patrol, L"a Patrol");
    Assert::AreEqual(SubAtColumn(120), orders[0].operands[1], L"anchored at the FIRST point");
  }

  /// §6, until K4's construction panel: a hotkey plus a click places a structure. The operand is a
  /// CELL and not a point, because placement is on the grid (GameShared/Order.h).
  TEST_METHOD(PlacingAStructureNamesACellAndNotAPoint)
  {
    Session session;
    session.selected = {Unarmed(1)};
    session.input.ArmStructure(3);
    const std::vector<Outpost::Order> orders = session.LeftClick(164, 100);
    Assert::AreEqual(std::size_t{1}, orders.size(), L"one placement, whatever is selected");
    Assert::IsTrue(orders[0].kind == Outpost::OrderKind::PlaceStructure, L"a PlaceStructure");
    Assert::AreEqual(std::int32_t{3}, orders[0].operands[0], L"of the row armed");
    Assert::AreEqual(std::int32_t{1}, orders[0].operands[1], L"at cell x: 64 world units is one cell");
  }

  /// §7: "Escape cancels an armed order, then a modal panel, then the pause menu". It is peeled,
  /// so this object takes it only while something of its own is open.
  TEST_METHOD(EscapePeelsTheArmedOrderAndNothingElse)
  {
    Session session;
    session.selected = {Armed(1)};
    (void)session.Press('R');
    Assert::AreEqual(std::size_t{0}, session.Press(0x1B).size(), L"Escape sends no order");
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::None, L"and the armed order is gone");

    const std::vector<Outpost::Order> orders = session.LeftClick(140, 100);
    Assert::AreEqual(std::size_t{0}, orders.size(), L"the next left click selects instead of ordering");
  }

  /// §6: "Escape or a right click disarms it." One click back to the ordinary cursor, rather than
  /// one structure to a refund.
  TEST_METHOD(ARightClickWhileArmedDisarmsAndOrdersNothing)
  {
    Session session;
    session.selected = {Unarmed(1)};
    session.input.ArmStructure(2);
    const std::vector<Outpost::Order> orders = session.RightClick(120, 100);
    Assert::AreEqual(std::size_t{0}, orders.size(), L"no Move and no placement");
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::None, L"only a disarm");
  }

  /// §5's rule, which orders obey as much as selection does: a click the interface has already
  /// answered must not reach through the panel into the world.
  TEST_METHOD(AClickInsideAPanelOrdersNothing)
  {
    Session session;
    session.selected = {Unarmed(1)};
    session.blocked = {Outpost::PickBox{90, 90, 130, 130}};
    Assert::AreEqual(std::size_t{0}, session.RightClick(100, 100).size(), L"the panel took it");
  }

  /// The one conversion in this file, pinned: a ground point is in SUBUNITS, 256 to the world unit,
  /// because that is the unit every positional operand of GameShared/Order.h is in. Getting it wrong sends
  /// a Move 256 times too far and the simulation obeys it.
  TEST_METHOD(AGroundPointIsInSubunits)
  {
    const Outpost::PickRay ray = Outpost::RayThrough(TopDownCamera(), 150, 100);
    std::int32_t x = 0;
    std::int32_t z = 0;
    Assert::IsTrue(Outpost::GroundPoint(ray, x, z), L"the ray meets the ground");
    Assert::AreEqual(SubAtColumn(150), x, L"fifty and a half world units, through the pixel's centre");
    Assert::AreEqual(SubAtRow(100), z, L"and the click was all but on the z axis");
  }
  TEST_METHOD(ADevicesAbilitiesAreItsDesignsAndNotItsOwn)
  {
    // The wire carries a design INDEX and the seat's DesignState says what that index names
    // (GameShared/Records.h), so this is the one place a click on a gunner comes to know it can shoot.
    const Outpost::ContentTree tables = AbilityTables();
    Outpost::DesignStore designs;
    designs.Remember(Design(0, 0, {GUN_ROW}));
    designs.Remember(Design(0, 1, {BUILDER_ROW}));
    designs.Remember(Design(0, 2, {GUN_ROW, BUILDER_ROW}));
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 0, 0);
    devices[11] = DeviceOf(11, 0, 1);
    devices[12] = DeviceOf(12, 0, 2);
    const std::map<std::uint32_t, Outpost::ReplicaStructure> structures;

    std::vector<std::uint32_t> ids = {10, 11, 12};
    std::vector<Outpost::SelectedObject> selected;
    Outpost::AbilitiesOf(devices, structures, designs, tables, ids, selected);

    Assert::AreEqual(std::size_t{3}, selected.size(), L"three devices");
    Assert::IsTrue(selected[0].weapon && !selected[0].builder, L"the gunner shoots and does not build");
    Assert::IsTrue(!selected[1].weapon && selected[1].builder, L"the builder builds and does not shoot");
    Assert::IsTrue(selected[2].weapon && selected[2].builder, L"a device of both does both");
  }

  TEST_METHOD(ABuilderRowWithNoBuildPowerIsNotABuilder)
  {
    // A legal row (GameShared/ComponentDesc.h) and the case that tells "carries build power" apart
    // from "is spelled Builder": a selection of these would otherwise be offered a Build the
    // simulation refuses for every one of them.
    const Outpost::ContentTree tables = AbilityTables();
    Outpost::DesignStore designs;
    designs.Remember(Design(0, 0, {IDLE_BUILDER_ROW}));
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 0, 0);
    const std::map<std::uint32_t, Outpost::ReplicaStructure> structures;

    std::vector<std::uint32_t> ids = {10};
    std::vector<Outpost::SelectedObject> selected;
    Outpost::AbilitiesOf(devices, structures, designs, tables, ids, selected);
    Assert::AreEqual(std::size_t{1}, selected.size(), L"one device");
    Assert::IsFalse(selected[0].builder, L"a builder with no build power builds nothing");
  }

  TEST_METHOD(ASensorIsNeitherAWeaponNorABuilder)
  {
    // The case that keeps "a weapon is a module with no system kind" from reading as "a weapon is
    // any module": a scout of one sensor would otherwise be given an Attack on every right click.
    const Outpost::ContentTree tables = AbilityTables();
    Outpost::DesignStore designs;
    designs.Remember(Design(0, 0, {SENSOR_ROW}));
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 0, 0);
    const std::map<std::uint32_t, Outpost::ReplicaStructure> structures;

    std::vector<std::uint32_t> ids = {10};
    std::vector<Outpost::SelectedObject> selected;
    Outpost::AbilitiesOf(devices, structures, designs, tables, ids, selected);
    Assert::IsTrue(!selected[0].weapon && !selected[0].builder, L"a sensor is neither");
  }

  TEST_METHOD(AStructureIsSelectedAndHasNeitherAbility)
  {
    // §6: "a structure takes no order on the world". It is kept in the list rather than dropped,
    // because a mixed selection gives its devices their orders and its structures none.
    const Outpost::ContentTree tables = AbilityTables();
    Outpost::DesignStore designs;
    designs.Remember(Design(0, 0, {GUN_ROW}));
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 0, 0);
    std::map<std::uint32_t, Outpost::ReplicaStructure> structures;
    structures[20].state.id = 20;

    std::vector<std::uint32_t> ids = {10, 20};
    std::vector<Outpost::SelectedObject> selected;
    Outpost::AbilitiesOf(devices, structures, designs, tables, ids, selected);
    Assert::AreEqual(std::size_t{2}, selected.size(), L"the structure is still selected");
    Assert::IsTrue(selected[1].kind == Outpost::ObjectKind::Structure, L"and is known to be one");
    Assert::IsTrue(!selected[1].weapon && !selected[1].builder, L"and can do neither");
  }

  TEST_METHOD(AnIdTheReplicaHasForgottenIsLeftOutRatherThanGuessedAt)
  {
    // A device that died between the click and this frame. Leaving it in would put its id in every
    // order the selection gives, and every one would come back refused with NotOwned - which reads
    // as the game ignoring the commander rather than as a unit having died.
    const Outpost::ContentTree tables = AbilityTables();
    Outpost::DesignStore designs;
    designs.Remember(Design(0, 0, {GUN_ROW}));
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 0, 0);
    const std::map<std::uint32_t, Outpost::ReplicaStructure> structures;

    std::vector<std::uint32_t> ids = {10, 99};
    std::vector<Outpost::SelectedObject> selected;
    Outpost::AbilitiesOf(devices, structures, designs, tables, ids, selected);
    Assert::AreEqual(std::size_t{1}, selected.size(), L"the forgotten id is not there");
    Assert::AreEqual(std::uint32_t{10}, selected[0].id, L"and the one that is left is the right one");
  }

  TEST_METHOD(ADeviceWhoseDesignHasNotArrivedKeepsItsPlaceAndGetsNoAbilities)
  {
    // The frame or two after a rejoin, before the seat's designs have been re-sent. Dropping it
    // would unselect the commander's army for him; guessing would offer it orders it cannot take.
    const Outpost::ContentTree tables = AbilityTables();
    const Outpost::DesignStore designs;
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 0, 7);
    const std::map<std::uint32_t, Outpost::ReplicaStructure> structures;

    std::vector<std::uint32_t> ids = {10};
    std::vector<Outpost::SelectedObject> selected;
    Outpost::AbilitiesOf(devices, structures, designs, tables, ids, selected);
    Assert::AreEqual(std::size_t{1}, selected.size(), L"still selected");
    Assert::IsTrue(!selected[0].weapon && !selected[0].builder, L"and neither ability is invented");
  }

  TEST_METHOD(ADesignNamingARowTheTablesDoNotCarryIsSkippedAndTheRestIsRead)
  {
    // C1's validator is what refuses that content; this only has to not read past the end of the
    // module table, and to still find the gun beside it.
    //
    // THE ROW IS EXACTLY ONE PAST THE END and not a large number, because the bound is the thing
    // being tested: a `>` where a `>=` belongs reads modules[size()] and is caught by nothing when
    // the bad row is 99. Measured - with 99 the mutation survived.
    const Outpost::ContentTree tables = AbilityTables();
    const auto pastTheEnd = static_cast<std::uint32_t>(tables.components.modules.size());
    Outpost::DesignStore designs;
    designs.Remember(Design(0, 0, {pastTheEnd, GUN_ROW}));
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 0, 0);
    const std::map<std::uint32_t, Outpost::ReplicaStructure> structures;

    std::vector<std::uint32_t> ids = {10};
    std::vector<Outpost::SelectedObject> selected;
    Outpost::AbilitiesOf(devices, structures, designs, tables, ids, selected);
    Assert::IsTrue(selected[0].weapon, L"the row beside it is still read");
  }

  TEST_METHOD(TheOrderGivenIsTheOrderTheIdsCameIn)
  {
    // Selection hands its ids ascending and OrderInput walks them in step with the caller's own
    // list; a helper that sorted or reordered would put one device's abilities on another.
    const Outpost::ContentTree tables = AbilityTables();
    Outpost::DesignStore designs;
    designs.Remember(Design(0, 0, {GUN_ROW}));
    designs.Remember(Design(0, 1, {BUILDER_ROW}));
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 0, 1);
    devices[11] = DeviceOf(11, 0, 0);
    const std::map<std::uint32_t, Outpost::ReplicaStructure> structures;

    std::vector<std::uint32_t> ids = {11, 10};
    std::vector<Outpost::SelectedObject> selected;
    Outpost::AbilitiesOf(devices, structures, designs, tables, ids, selected);
    Assert::AreEqual(std::uint32_t{11}, selected[0].id, L"the caller's order is kept");
    Assert::IsTrue(selected[0].weapon, L"and 11 is the gunner");
    Assert::IsTrue(selected[1].builder, L"and 10 is the builder");
  }

  TEST_METHOD(TwoSeatsDesignsAtTheSameIndexAreNotMixedUp)
  {
    // A design index is the SEAT's, not the match's (GameShared/Records.h), so an enemy device selected as
    // an inspection must read its own commander's design and not this one's at the same number.
    const Outpost::ContentTree tables = AbilityTables();
    Outpost::DesignStore designs;
    designs.Remember(Design(0, 0, {BUILDER_ROW}));
    designs.Remember(Design(1, 0, {GUN_ROW}));
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 1, 0);
    const std::map<std::uint32_t, Outpost::ReplicaStructure> structures;

    std::vector<std::uint32_t> ids = {10};
    std::vector<Outpost::SelectedObject> selected;
    Outpost::AbilitiesOf(devices, structures, designs, tables, ids, selected);
    Assert::IsTrue(selected[0].weapon && !selected[0].builder, L"seat 1's design and not seat 0's");
  }

  TEST_METHOD(TheAnswerReplacesTheLastOneRatherThanPilingOnToIt)
  {
    // The caller keeps one vector and fills it every frame, which is the whole reason this takes an
    // out parameter. A helper that appended would grow the selection by its own size every frame,
    // and the commander would watch one right click turn into a hundred orders.
    const Outpost::ContentTree tables = AbilityTables();
    Outpost::DesignStore designs;
    designs.Remember(Design(0, 0, {GUN_ROW}));
    designs.Remember(Design(0, 1, {BUILDER_ROW}));
    std::map<std::uint32_t, Outpost::ReplicaDevice> devices;
    devices[10] = DeviceOf(10, 0, 0);
    devices[11] = DeviceOf(11, 0, 1);
    const std::map<std::uint32_t, Outpost::ReplicaStructure> structures;

    std::vector<Outpost::SelectedObject> selected;
    std::vector<std::uint32_t> both = {10, 11};
    Outpost::AbilitiesOf(devices, structures, designs, tables, both, selected);
    Assert::AreEqual(std::size_t{2}, selected.size(), L"two the first time");

    std::vector<std::uint32_t> one = {11};
    Outpost::AbilitiesOf(devices, structures, designs, tables, one, selected);
    Assert::AreEqual(std::size_t{1}, selected.size(), L"and one the second, not three");
    Assert::AreEqual(std::uint32_t{11}, selected[0].id, L"the one that is left is this frame's");
  }
  TEST_METHOD(BuildArmsTheFirstStructureRowAndThenCyclesThroughTheRest)
  {
    // §7 gives Build ONE key and K4's panel is what will give each structure a button, so until
    // then the key has to reach all of them: a key that armed row 0 and nothing else would leave
    // five of the six structures unbuildable.
    Session session;
    Assert::IsTrue(session.Press(session.input.Keys().build).empty(), L"arming sends no order");
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::PlaceStructure, L"Build arms a placement");
    Assert::AreEqual(std::uint32_t{0}, session.input.ArmedStructure(), L"the first press arms row 0");
    static_cast<void>(session.Press(session.input.Keys().build));
    Assert::AreEqual(std::uint32_t{1}, session.input.ArmedStructure(), L"the second steps on");
    static_cast<void>(session.Press(session.input.Keys().build));
    Assert::AreEqual(std::uint32_t{2}, session.input.ArmedStructure(), L"and the third");
    static_cast<void>(session.Press(session.input.Keys().build));
    Assert::AreEqual(std::uint32_t{0}, session.input.ArmedStructure(), L"and it wraps rather than running off the table");
  }

  TEST_METHOD(BuildAfterAnotherArmedOrderStartsAtTheFirstRowAgain)
  {
    // The cycle is the PLACEMENT's and not the keyboard's: a commander who armed Move and then
    // pressed Build means "build the first thing", not "build whatever the counter had reached".
    Session session;
    static_cast<void>(session.Press(session.input.Keys().build));
    static_cast<void>(session.Press(session.input.Keys().build));
    Assert::AreEqual(std::uint32_t{1}, session.input.ArmedStructure(), L"the counter had moved on");
    static_cast<void>(session.Press(session.input.Keys().move));
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::Move, L"Move took the arming");
    static_cast<void>(session.Press(session.input.Keys().build));
    Assert::AreEqual(std::uint32_t{0}, session.input.ArmedStructure(), L"and Build starts again at the first row");
  }

  TEST_METHOD(BuildWithNoStructureTableArmsNothing)
  {
    // A frame before the content has arrived. Arming row 0 of a table with no rows would send a
    // PlaceStructure naming a structure that does not exist, which the simulation answers Malformed
    // - a client fault rather than a game one, and one worth not committing.
    Session session;
    session.structureRowCount = 0;
    static_cast<void>(session.Press(session.input.Keys().build));
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::None, L"nothing is armed");
  }

  TEST_METHOD(AnArmedBuildIsIssuedByTheNextLeftClickAsTheCellItNames)
  {
    // The hotkey half of §6's "hotkey plus click for structure placement", end to end: the key
    // arms, the click names a cell, and the order carries the row the key had reached.
    Session session;
    static_cast<void>(session.Press(session.input.Keys().build));
    static_cast<void>(session.Press(session.input.Keys().build));
    const std::vector<Outpost::Order> orders = session.LeftClick(150, 60);
    Assert::AreEqual(std::size_t{1}, orders.size(), L"one placement");
    Assert::IsTrue(orders[0].kind == Outpost::OrderKind::PlaceStructure, L"a placement");
    Assert::AreEqual(1, orders[0].operands[0], L"of the row the key had reached");
    Assert::AreEqual(SubAtColumn(150) / Neuron::SUBUNITS_PER_CELL, orders[0].operands[1], L"at the cell under the click");
    Assert::AreEqual(SubAtRow(60) / Neuron::SUBUNITS_PER_CELL, orders[0].operands[2], L"and its row");
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::None, L"and it disarmed itself");
  }
  TEST_METHOD(TheCallersGroundPointIsUsedInsteadOfThePlaneAtZeroWhenItHasOne)
  {
    // m1-vertical-slice/K7: the executable casts ONE ray, against the heightfield, so the point a
    // right click orders is the point the ground cursor is drawn at. Over ground 200 world units
    // up at the camera's default pitch the two answers are 400 units apart - six cells - which is
    // a Move that lands somewhere the commander did not point.
    Session session;
    session.selected = {Unarmed(7)};
    session.groundKnown = true;
    session.groundX = Sub(1234.0f);
    session.groundZ = Sub(-567.0f);
    const std::vector<Outpost::Order> orders = session.RightClick(150, 60);
    Assert::AreEqual(std::size_t{1}, orders.size(), L"one Move");
    Assert::AreEqual(Sub(1234.0f), orders[0].operands[1], L"the caller's x and not the plane's");
    Assert::AreEqual(Sub(-567.0f), orders[0].operands[2], L"the caller's z and not the plane's");
  }

  TEST_METHOD(WithNoGroundPointFromTheCallerThePlaneAtZeroIsStillTheAnswer)
  {
    // The window before the landscape has arrived, and every case above it: a frame that names no
    // ground is answered by the plane, which is what Replica can do on its own.
    Session session;
    session.selected = {Unarmed(7)};
    const std::vector<Outpost::Order> orders = session.RightClick(150, 60);
    Assert::AreEqual(std::size_t{1}, orders.size(), L"one Move");
    Assert::AreEqual(SubAtColumn(150), orders[0].operands[1], L"the plane's x");
    Assert::AreEqual(SubAtRow(60), orders[0].operands[2], L"the plane's z");
  }

  TEST_METHOD(AnArmedOrderTakesTheCallersGroundPointToo)
  {
    // Both paths through this file read the ground, and an armed Build that used the plane while a
    // right click used the heightfield would lay the structure a few cells from the ghost.
    Session session;
    session.selected = {Unarmed(7)};
    session.groundKnown = true;
    session.groundX = Sub(2000.0f);
    session.groundZ = Sub(3000.0f);
    static_cast<void>(session.Press(session.input.Keys().build));
    const std::vector<Outpost::Order> orders = session.LeftClick(150, 60);
    Assert::AreEqual(std::size_t{1}, orders.size(), L"one placement");
    Assert::AreEqual(Sub(2000.0f) / Neuron::SUBUNITS_PER_CELL, orders[0].operands[1], L"the caller's cell");
    Assert::AreEqual(Sub(3000.0f) / Neuron::SUBUNITS_PER_CELL, orders[0].operands[2], L"and its row");
  }

  // ── Arm: §9.1's buttons reaching the same armed order §7's keys do ──────────────────────────

  TEST_METHOD(ArmArmsTheOrderAButtonAsksFor)
  {
    Session session;
    session.input.Arm(Outpost::ArmedOrder::AttackMove);
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::AttackMove, L"armed by the button");
    session.selected = {Unarmed(7)};
    const std::vector<Outpost::Order> orders = session.LeftClick(150, 60);
    Assert::AreEqual(std::size_t{1}, orders.size(), L"and the next click issues it");
    Assert::IsTrue(orders[0].kind == Outpost::OrderKind::AttackMove, L"as an attack-move");
  }

  TEST_METHOD(ArmingWhatIsArmedDisarmsIt)
  {
    // A second press on a lit button puts it out, which is what a toggle is and what Escape does
    // from the keyboard (§6).
    Session session;
    session.input.Arm(Outpost::ArmedOrder::Move);
    session.input.Arm(Outpost::ArmedOrder::Move);
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::None, L"disarmed");
  }

  TEST_METHOD(ArmingAnotherOrderReplacesTheFirst)
  {
    Session session;
    session.input.Arm(Outpost::ArmedOrder::Move);
    session.input.Arm(Outpost::ArmedOrder::Patrol);
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::Patrol, L"the second wins");
  }

  TEST_METHOD(ArmRefusesAPlacementBecauseItCarriesNoRow)
  {
    // A placement needs the structure row as well, so it goes through ArmStructure; arming one here
    // would lay a plan of whatever row was last used.
    Session session;
    session.input.Arm(Outpost::ArmedOrder::PlaceStructure);
    Assert::IsTrue(session.input.Armed() == Outpost::ArmedOrder::None, L"refused rather than armed");
  }

  TEST_METHOD(ArmForgetsAPatrolsFirstPoint)
  {
    // A patrol half given and then re-armed must not take its old first point as the new one's.
    Session session;
    session.selected = {Unarmed(7)};
    session.input.Arm(Outpost::ArmedOrder::Patrol);
    Assert::AreEqual(std::size_t{0}, session.LeftClick(150, 60).size(), L"the first click gives nothing");
    Assert::IsTrue(session.input.AwaitingSecondPoint(), L"it is waiting for the far point");
    session.input.Arm(Outpost::ArmedOrder::Patrol);
    Assert::IsFalse(session.input.AwaitingSecondPoint(), L"and re-arming forgets it");
  }
};

} // namespace ReplicaTests
