#pragma once

#include "Match.h"

#include "OrderInput.h"
#include "Picking.h"
#include "Selection.h"
#include "WarningLine.h"

#include "InterfaceDesc.h"
#include "StructureDesc.h"

#include "FrameInput.h"
#include "GroundRay.h"
#include "InputEvent.h"
#include "PointerMode.h"
#include "ScaleMode.h"
#include "UiDraw.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

// The commander at the keyboard (Design/Interface.md §4, §5, §6 and §10; m1-vertical-slice/G1b):
// what a frame's input does to the selection and the orders, which pointer mode the game is in,
// and the overlay that tells him what happened.
//
// IT IS IN THE EXECUTABLE BECAUSE IT HAS TO BE, and that is the layering rather than a preference.
// It needs Client's authored-pixel arithmetic (ScaleMode.h) and Replica's picking and orders, and
// ADR-001 puts those two beside each other: Client and Replica are siblings over Core, so nothing
// but an executable may hold both. What COULD be tested is already out of here - Selection,
// OrderInput, WarningLine and PlacementPreview in Replica, PointerMode in Client, each with its own
// suite - and what is left is the wiring between them, which has nothing to get wrong that a
// compiler will not catch.
//
// NO DIRECT3D AND NO DirectXMath, though. The camera arrives as a PickCamera, which is sixteen
// floats and its inverse (GameClient/Picking.h), so this file can be built and stepped through a whole
// match on any machine even though the pass that draws its quads cannot.
//
// NOTHING HERE TOUCHES Sim, which is G1b's acceptance in as many words: every order it makes is an
// Order handed to Match::Submit, and the simulation is the host's.

namespace Outpost
{

/// Where the two overlays sit, in authored pixels (Design/Interface.md §10). Named rather than
/// written into the drawing, because §5 has to refuse a world click inside them and the two
/// rectangles must be the same one.
inline constexpr Neuron::UiRect MATCH_END_RECT{560, 380, 800, 320};
inline constexpr Neuron::UiRect PAUSE_MENU_RECT{760, 420, 400, 240};

/// Where the refusal line is drawn: "one line of warning text centred at y 720" (§6).
inline constexpr std::int32_t WARNING_LINE_Y_PIXELS = 720;

/// And where "what is armed" is drawn, a line above it: a commander cycling the Build key has to be
/// told which structure he is about to lay, and §4's cursor says Build rather than which build.
inline constexpr std::int32_t ARMED_LINE_Y_PIXELS = 696;

/// F10, which opens the pause menu from either pointer mode (§7's table; the owner moved it off
/// Escape on 2026-09-19).
inline constexpr std::uint8_t KEY_PAUSE_MENU = 0x79;
inline constexpr std::uint8_t KEY_ESCAPE = 0x1B;

class Operator
{
public:
  /// One frame, with the window's pixels already turned into the frame the interface is laid out
  /// in. The caller owns the conversion because it owns the window (NeuronClient/ScaleMode.h's
  /// AuthoredFromClient), and the camera arrives as matrices for the reason the file header gives.
  struct Frame
  {
    const Neuron::FrameInput* input = nullptr;
    /// Where the pointer is in AUTHORED pixels, and whether it is inside the picture at all. A
    /// pointer in the letterbox bars is over no panel and over no world (§4).
    std::int32_t pointerX = 0;
    std::int32_t pointerY = 0;
    bool pointerInside = false;
    PickCamera camera;
    /// The commander's own landscape, for the one ray this frame casts. Null before the join has
    /// answered, and then the orders fall back to Replica's plane at y = 0.
    const Neuron::HeightView* terrain = nullptr;
    std::int64_t nowMilliseconds = 0;
    /// THE PANELS' RECTANGLES, which §5 refuses to cast a picking ray through (Hud::BlockedBy).
    /// They arrive from outside rather than being known here because the panels are K4's object and
    /// this one is G1b's, and the two meet in App - which is also where the input sink that
    /// consumes the click itself lives, so this is the SECOND half of the same rule: the sink stops
    /// a click on a panel becoming a click on the world, and this stops the drag rectangle and the
    /// hover reaching through a panel on a frame where no button moved at all.
    std::span<const PickBox> panels;
  };

  /// Reads the frame, moves the selection, and submits whatever orders it made. _match is advanced
  /// by its own loop; what this does to it is select, submit and pause.
  void Advance(const Frame& _frame, const ContentTree& _content, Match& _match);

  /// The overlay this commander is owed: the drag rectangle, the refusal line, the pause menu and
  /// the match-end panel. The world's own cursor is not here - in point mode it is K4's six icons
  /// and in aim mode it is K7's ring on the ground.
  void BuildOverlay(const ChromePalette& _palette, std::span<const StructureDesc> _structures, const Match& _match,
                    std::int64_t _nowMilliseconds, std::vector<Neuron::UiQuad>& _outQuads) const;

  [[nodiscard]] Neuron::PointerMode Mode() const noexcept
  {
    return m_mode;
  }

  /// True once the commander has asked to leave, from either overlay's Quit.
  [[nodiscard]] bool QuitRequested() const noexcept
  {
    return m_quit;
  }

  [[nodiscard]] bool MenuOpen() const noexcept
  {
    return m_menuOpen;
  }

  [[nodiscard]] ArmedOrder Armed() const noexcept
  {
    return m_orders.Armed();
  }

  /// Arms a structure row, for §7.1's buttons: "clicking one enters placement". The panel asks and
  /// this object arms, because arming is OrderInput's state and the panels hold none of the game's.
  void ArmStructure(std::uint32_t _row) noexcept
  {
    m_orders.ArmStructure(_row);
  }

  /// And the three orders that take a point, for §9.1's buttons.
  void Arm(ArmedOrder _armed) noexcept
  {
    m_orders.Arm(_armed);
  }

  /// Narrows the selection to one device, for §8's portrait grid. It does not go through the world:
  /// see Selection::Hold.
  void SelectOnly(std::uint32_t _id)
  {
    const std::array<std::uint32_t, 1> one = {_id};
    m_selection.Hold(one);
  }

  /// Moves the selection's devices to a point in WORLD UNITS, for the last row of §6's table: "the
  /// minimap | devices | Move to the landscape point the minimap pixel names". It is here and not
  /// in the panel because it makes orders, and every order this commander gives comes from one
  /// object.
  void MoveTo(float _worldX, float _worldZ, Match& _match);

  /// What the selection can do, as this frame read it (GameClient/OrderInput.h's AbilitiesOf). The
  /// panels want the same answer and it is one walk of the replica, so it is read once here rather
  /// than a second time beside them.
  [[nodiscard]] std::span<const SelectedObject> Abilities() const noexcept
  {
    return m_abilities;
  }

  [[nodiscard]] std::span<const std::uint32_t> Selected() const noexcept
  {
    return m_selection.Ids();
  }

  /// WHERE THIS FRAME'S RAY MET THE GROUND, cast once and answered to everything that needs it:
  /// the orders above, and the ring the cursor pass draws (m1-vertical-slice/K7). A miss is the
  /// ray leaving the landscape, and the cursor is hidden rather than left where it was.
  [[nodiscard]] const Neuron::GroundHit& Ground() const noexcept
  {
    return m_ground;
  }

  /// The structure row a placement is armed with, for the footprint ghost. Meaningless unless
  /// Armed() is PlaceStructure.
  [[nodiscard]] std::uint32_t ArmedStructure() const noexcept
  {
    return m_orders.ArmedStructure();
  }

private:
  /// The rectangles a world click may not be cast through this frame (§5): the frame's panels and
  /// whichever overlays are open. Rebuilt every frame into m_blocked, which is scratch.
  void BlockedBy(const Frame& _frame, const Match& _match, std::vector<PickBox>& _outBoxes) const;
  /// F10 and Escape, in the order §7 peels them. Returns true when the frame's input was the
  /// menu's and the world should see none of it.
  [[nodiscard]] bool TakeMenuKeys(const Frame& _frame, Match& _match);

  Neuron::GroundHit m_ground{};
  Neuron::PointerMode m_mode = Neuron::PointerMode::Aim;
  /// Where the pointer was when the menu opened, so that Resume puts it back (§10).
  Neuron::PointerMode m_modeBeforeMenu = Neuron::PointerMode::Aim;
  Selection m_selection;
  OrderInput m_orders;
  WarningLine m_warning;
  bool m_menuOpen = false;
  bool m_quit = false;
  /// Scratch, so that a frame allocates nothing after the first.
  mutable std::vector<PickBox> m_blocked;
  std::vector<SelectedObject> m_abilities;
  std::vector<Order> m_made;
};

} // namespace Outpost
