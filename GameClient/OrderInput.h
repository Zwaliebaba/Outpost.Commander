#pragma once

#include "DesignStore.h"
#include "Order.h"
#include "Picking.h"
#include "ReplicaObject.h"

#include "ContentTree.h"

#include <map>

#include <cstdint>
#include <span>
#include <vector>

// Turning what the commander did into orders (Design/Interface.md §6; m1-vertical-slice/G1b).
//
// IN Replica AND NOT IN THE EXECUTABLE, for the reason Selection.h records: a test project may not
// list a source outside its own directory (Build/CheckProjectFiles.py), and an .exe exports nothing
// to link against, so logic that deserves tests lives in a library. This one holds interface state
// - what is armed, whether a patrol awaits its far point - which is the one thing that reads oddly
// here; RenderViewBuilder next door holds state of its own for the same reason, that the thing
// reading the replica has to remember what it last saw.
//
// NOTHING HERE TOUCHES Sim. Every gesture becomes an Outpost::Order that Match::Submit hands to
// Net, which is G1b's acceptance line and the thing that makes K4's panels a matter of emitting the
// same records. The rule is easy to keep here and easy to lose later, so it is worth saying where
// the temptation is: this object can see what is under the cursor and it CANNOT see whether the
// order will be accepted. "He cannot afford it" is stage 1's judgement and comes back as a
// rejection (§6); guessing at it here would put a second copy of the rules in the client.
//
// NO DEVICE AND NO WINDOW, for the same reason Selection.h has none: §6's default-order table is
// seven rows of "what is under the cursor, crossed with what is selected", and a table is a unit
// test. What it needs of the replica is reduced to SelectedObject before it gets here, because
// "does this selection have a weapon" is a question about a design and a content row, and asking it
// once a frame beats asking it once an order.
//
// AN ORDER NAMES ONE OBJECT (GameShared/Order.h), so a selection of twelve devices is twelve orders. That
// is the simulation's shape and not a convenience: it keeps the record fixed-layout and the
// validation per object, and it is why Advance returns a vector rather than one Order.

namespace Outpost
{

/// One selected object as the order rules read it: what it is and what it can do. Built from the
/// replica's device and its seat's design once a frame (AbilitiesOf below).
struct SelectedObject
{
  std::uint32_t id = 0;
  ObjectKind kind = ObjectKind::Device;
  bool weapon = false;  ///< Carries a module that can shoot
  bool builder = false; ///< Carries build power, so it can construct and repair
};

/// The order armed for the next left click on the world (§6). None is the ordinary state, in which
/// a left click selects instead.
enum class ArmedOrder : std::uint8_t
{
  None,
  Move,
  Patrol,
  AttackMove,
  PlaceStructure
};

/// One frame as OrderInput reads it. Authored pixels, as Selection uses.
struct OrderFrame
{
  PickCamera camera;
  std::span<const PickCandidate> candidates;
  std::span<const PickBox> blocked;
  std::span<const SelectedObject> selected;
  std::uint8_t ownSeat = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  bool leftPressed = false;  ///< The left button went down this frame
  bool rightPressed = false; ///< The right button went down this frame
  /// The key edges of the frame, indexed by virtual-key code: +1 pressed, -1 released, 0 neither
  /// (NeuronClient/FrameInput.h). Empty is a frame with no keyboard, which every test but the hotkey ones
  /// passes.
  std::span<const std::int8_t> keyEdges;
  /// How many structure rows the tables carry, for the Build hotkey below. Zero is a frame with no
  /// content, and the key then arms nothing rather than a row that does not exist.
  std::uint32_t structureRowCount = 0;
  /// WHERE THE RAY MEETS THE GROUND, when the caller has an answer better than this file's
  /// (m1-vertical-slice/K7). GroundPoint below intersects the plane y = 0, which is the only thing
  /// Replica can do: the heightfield ray is NeuronClient/GroundRay.h's and Replica may not include Client
  /// (ADR-001 makes them siblings). The executable holds both and casts ONE ray, so the point a
  /// right click orders and the point the ground cursor is drawn at are the same point.
  ///
  /// IT IS WORTH SIX CELLS. Over ground 200 world units up, at the camera's default pitch of 26.6
  /// degrees, the plane at y = 0 is 400 world units further along the ray than the ground is.
  bool groundKnown = false;
  std::int32_t groundX = 0; ///< Subunits, as every positional operand of GameShared/Order.h is
  std::int32_t groundZ = 0;
};

/// The hotkeys of Design/Interface.md §7, as virtual-key codes. A struct rather than constants
/// because §7 says the bindings live in Preferences.json and that the binding layer is M1's: when
/// it lands it fills one of these, and nothing else here changes.
struct OrderKeys
{
  std::uint8_t move = 'M';
  std::uint8_t patrol = 'P';
  std::uint8_t attackMove = 'R';
  /// Build. ONE KEY FOR EVERY STRUCTURE, which is what §7's table gives it, so until K4's
  /// construction panel gives each row a button of its own the key CYCLES: each press arms the next
  /// structure row, and the armed row is what the interface shows. A key that armed row 0 and
  /// nothing else would make five of the six structures unreachable without a panel.
  std::uint8_t build = 'B';
  std::uint8_t stop = 'S';
  std::uint8_t holdPosition = 'H';
  std::uint8_t cancel = 0x1B; ///< Escape, which §7 peels: an armed order first
};

class OrderInput
{
public:
  /// One frame. Appends the orders it made to _outOrders, which the caller submits; nothing is
  /// appended on a frame that gave none.
  void Advance(const OrderFrame& _frame, std::vector<Order>& _outOrders);

  /// What is armed now, for the cursor (§7) and the footprint ghost.
  [[nodiscard]] ArmedOrder Armed() const noexcept
  {
    return m_armed;
  }
  /// The structure row PlaceStructure will place, meaningless unless PlaceStructure is armed.
  [[nodiscard]] std::uint32_t ArmedStructure() const noexcept
  {
    return m_structureRow;
  }
  /// Arms the placement of a structure row, which until K4's construction panel is a hotkey's job
  /// and afterwards a button's.
  void ArmStructure(std::uint32_t _row) noexcept;

  /// Arms one of the three orders that take a point, for §9.1's buttons: "an order that arms (§6)
  /// fills accent while armed". PlaceStructure is ArmStructure's, because it needs the row as well,
  /// and None disarms - which is what a second click on a lit button does.
  void Arm(ArmedOrder _armed) noexcept;

  /// The far point of a Patrol is a second click; this says the first has been taken.
  [[nodiscard]] bool AwaitingSecondPoint() const noexcept
  {
    return m_awaitingSecondPoint;
  }

  void Disarm() noexcept;

  /// The bindings this object reads. The default is §7's table.
  [[nodiscard]] OrderKeys& Keys() noexcept
  {
    return m_keys;
  }

private:
  void DefaultOrder(const OrderFrame& _frame, std::vector<Order>& _outOrders);
  void ArmedAt(const OrderFrame& _frame, std::vector<Order>& _outOrders);

  OrderKeys m_keys;
  ArmedOrder m_armed = ArmedOrder::None;
  std::uint32_t m_structureRow = 0;
  bool m_awaitingSecondPoint = false;
  std::int32_t m_firstX = 0;
  std::int32_t m_firstZ = 0;
};

/// What the selected ids can do, for the default-order table above: each id the replica still
/// holds, in the order it was given, with the two abilities §6's table asks about read off the
/// seat's own design. An id the replica has forgotten is left out rather than guessed at - a
/// device that died between the click and this frame can take no order at all.
///
/// A DEVICE'S ABILITIES ARE ITS DESIGN'S AND NOT ITS OWN, which is why this needs the content
/// tables as well as the replica: the wire carries a device's design INDEX (GameShared/Records.h's
/// DeviceState::design) and the seat's DesignState says which component rows that index names.
/// A design the replica has not been sent yet - which happens for a frame or two after a rejoin -
/// leaves the object with neither ability rather than dropping it, because it is still selected and
/// still takes a Move.
///
/// A STRUCTURE IS SELECTED AND HAS NEITHER, which §6 relies on: "a structure takes no order on the
/// world". It is here so that a mixed selection gives its devices their orders and quietly gives
/// the structures none.
/// IT NAMES WHAT IT READS RATHER THAN TAKING THE REPLICA, which is PlacementPreview.h's rule next
/// door and for its reason: what a piece of interface logic may see is worth writing down, and a
/// test hands it exactly that instead of standing a client, a host and a simulation up to get one.
void AbilitiesOf(const std::map<std::uint32_t, ReplicaDevice>& _devices, const std::map<std::uint32_t, ReplicaStructure>& _structures,
                 const DesignStore& _designs, const ContentTree& _content, std::span<const std::uint32_t> _ids,
                 std::vector<SelectedObject>& _outSelected);

/// Whether a ray through this screen point lands on the world at all, or inside a panel §5 refuses
/// to cast through. Shared with Selection's rule by being the same arithmetic written once.
[[nodiscard]] bool OnTheWorld(std::span<const PickBox> _blocked, std::int32_t _x, std::int32_t _y) noexcept;

/// Where a ray meets the ground plane at y = 0, in SUBUNITS, which is the unit every positional
/// operand of GameShared/Order.h is in. False when the ray is parallel to the ground or points away from
/// it, which is a camera aimed at the sky and an order with nowhere to go.
[[nodiscard]] bool GroundPoint(const PickRay& _ray, std::int32_t& _outX, std::int32_t& _outZ) noexcept;

} // namespace Outpost
