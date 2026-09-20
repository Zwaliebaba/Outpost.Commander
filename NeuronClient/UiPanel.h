#pragma once

#include "UiLayout.h"
#include "UiWidgets.h"

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

// A panel: a rectangle at the authored resolution, a title, and the widgets in it
// (Design/Interface.md §2, §3). Panels are FIXED - §2 says "nothing moves", and no panel here
// slides, collapses or animates - so a panel is built once when a match starts and then only has
// its widgets' values written each frame.
//
// A PANEL CONSUMES WHAT LANDS ON IT, whether or not a widget took it. §4 is explicit: a right
// click on a panel does nothing and the panel consumes it, and §2 says picking refuses a ray whose
// origin is inside a panel rectangle. If a click on the panel's own background fell through, the
// player would order a unit to walk to whatever was behind the readout he just clicked, which is
// the one input mistake a real-time game cannot take back.
//
// IT DRAWS NOTHING. A panel is state and hit testing; NeuronClient/UiDraw.h turns it into quads and
// NeuronClient/UiPass.h draws them. That is what lets the whole of this file be tested on any machine,
// with no device and no window.

namespace Neuron
{

/// What an event did. `consumed` is the router's answer (NeuronClient/InputRouter.h); `action` is the
/// owner's, and is None for an event that was consumed by the panel but changed nothing.
struct UiEventResult
{
  bool consumed = false;
  UiAction action = UiAction::None;
  std::uint32_t widget = 0; ///< The widget's own id, or 0 when nothing acted
  std::int32_t value = 0;   ///< Selected: the row. Toggled: 1 for on, 0 for off

  [[nodiscard]] constexpr bool operator==(const UiEventResult&) const noexcept = default;
};

class UiPanel
{
public:
  UiPanel() = default;
  UiPanel(const UiRect& _rect, std::string _title);

  [[nodiscard]] const UiRect& Rect() const noexcept
  {
    return m_rect;
  }

  [[nodiscard]] const std::string& Title() const noexcept
  {
    return m_title;
  }

  /// A panel whose title says what it is looking at rather than what it is: Design/Interface.md §8
  /// draws REMEMBERED in the selection panel's title strip for a ghost structure, and §3 gives the
  /// strip no widget of its own to put it in. Reset does not touch it, so a panel rebuilt every
  /// frame keeps whatever title it was last given until it is given another.
  void SetTitle(std::string _title)
  {
    m_title = std::move(_title);
  }

  [[nodiscard]] bool Visible() const noexcept
  {
    return m_visible;
  }

  void SetVisible(bool _visible) noexcept
  {
    m_visible = _visible;
  }

  /// Adds a widget and gives back its id, so that a caller can add without inventing one. An id of
  /// 0 on the widget takes the next free number; a non-zero one is kept.
  std::uint32_t Add(const UiWidget& _widget);

  [[nodiscard]] UiWidget* Find(std::uint32_t _id) noexcept;
  [[nodiscard]] const UiWidget* Find(std::uint32_t _id) const noexcept;

  [[nodiscard]] std::span<const UiWidget> Widgets() const noexcept
  {
    return m_widgets;
  }

  /// The interactive widget under an authored position, or nullptr. Readouts are skipped, so a
  /// label over a button does not shield it.
  [[nodiscard]] const UiWidget* WidgetAt(std::int32_t _x, std::int32_t _y) const noexcept;

  /// The pointer moved to an authored position. Tracks which widget is hovered, which is what the
  /// tooltip's dwell and a button's hover fill read.
  UiEventResult OnPointerMove(std::int32_t _x, std::int32_t _y) noexcept;

  /// A button went down at an authored position. A press on a widget arms it; a press anywhere on
  /// the panel is consumed.
  UiEventResult OnPointerDown(std::int32_t _x, std::int32_t _y, bool _leftButton);

  /// A button came up. A BUTTON ACTS ON THE RELEASE, and only inside the widget it was pressed in:
  /// pressing a button and sliding off it before letting go is how every interface in the world
  /// lets someone change their mind, and a button that fired on the press takes that away.
  UiEventResult OnPointerUp(std::int32_t _x, std::int32_t _y, bool _leftButton);

  /// A character arrived (WM_CHAR). It reaches the focused text field and nothing else; with no
  /// focus it is not consumed, so a hotkey still works when no field is being typed into.
  UiEventResult OnCharacter(std::uint32_t _character);

  /// The focused text field, or 0. A press outside every text field clears it.
  [[nodiscard]] std::uint32_t Focus() const noexcept
  {
    return m_focus;
  }

  void ClearFocus() noexcept
  {
    m_focus = 0;
  }

  [[nodiscard]] std::uint32_t Hovered() const noexcept
  {
    return m_hovered;
  }

  /// Forgets the pointer: the hover, the armed press and, optionally, the focus. Called when the
  /// window loses focus, so that a button does not stay lit under a pointer that has gone.
  void ForgetPointer() noexcept;

  /// Empties the widgets and KEEPS the pointer's state, for a panel whose contents are rebuilt from
  /// the replica every frame (Design/Interface.md §7 to §9; m1-vertical-slice/K4).
  ///
  /// THE PRESS AND THE HOVER SURVIVE ON PURPOSE, and that is safe only because a rebuilt panel
  /// gives its widgets the SAME ids: a button acts on the release and only inside the widget it was
  /// pressed in, so an id that moved to another control between the press and the release would
  /// fire the wrong one. A caller that rebuilds must therefore name its ids rather than take the
  /// numbers Add hands out - which is what the ids being the owner's handle is for.
  ///
  /// The next automatic id is reset too, so that a panel which does take Add's numbering gets the
  /// same ones every frame rather than counting up for ever.
  void Reset() noexcept;

private:
  [[nodiscard]] UiWidget* MutableAt(std::int32_t _x, std::int32_t _y) noexcept;

  UiRect m_rect;
  std::string m_title;
  std::vector<UiWidget> m_widgets;
  std::uint32_t m_nextId = 1;
  std::uint32_t m_hovered = 0;
  std::uint32_t m_pressed = 0; ///< The widget the left button went down in, for the release rule
  std::uint32_t m_focus = 0;
  bool m_visible = true;
};

} // namespace Neuron
