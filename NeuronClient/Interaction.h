#pragma once

namespace Neuron
{
// What a text field reads while it has the keyboard (ADR-017 decision 8): a character; Backspace, which takes one back;
// Enter, which ends typing and keeps the text; and Esc, which ends it and drops what was typed.
enum class FieldKey : std::uint8_t
{
  Character,
  Backspace,
  Enter,
  Escape
};

// A key that reached a text field, and the character it typed, if it is one.
struct FieldStroke
{
  FieldKey key = FieldKey::Character;
  char32_t character = 0;

  friend bool operator==(const FieldStroke&, const FieldStroke&) = default;
};

// What every widget of the canvas shares (ADR-100): the identifiers of the widgets built this frame; which widget is hot,
// under the pointer, which is active, pressed and holding the pointer until it is released, and which text field has the
// keyboard; and a small store per identifier, a scroll's offset, a caret, and the drop-down open, if any. Input is
// resolved against the display list drawn last, so that a press hits what the player saw. It holds no GPU state and
// reads no window, so its tests need neither.
class Interaction
{
public:
  // Starts a frame's build: no identifier is pushed and none has been claimed, and a field the last build did not build no
  // longer has the keyboard.
  void BeginBuild() noexcept;
  // _label's or _index's identifier under the identifier pushed last, or under none.
  [[nodiscard]] WidgetId IdOf(std::string_view _label) const noexcept;
  [[nodiscard]] WidgetId IdOf(std::uint64_t _index) const noexcept;
  // _label's identifier under _parent, whatever is pushed.
  [[nodiscard]] static WidgetId IdUnder(WidgetId _parent, std::string_view _label) noexcept;
  // Names what is built until PopId under _widget, such as a window's identifier.
  void PushId(WidgetId _widget);
  void PopId();
  // Records that _widget was built this frame: false when it already was, two widgets with one identifier, which the
  // canvas fails on (ADR-093).
  [[nodiscard]] bool Claim(WidgetId _widget);

  [[nodiscard]] WidgetId Hot() const noexcept
  {
    return m_hot;
  }
  [[nodiscard]] WidgetId Active() const noexcept
  {
    return m_active;
  }
  [[nodiscard]] WidgetId Focused() const noexcept
  {
    return m_focused;
  }

  // The pointer moved: the widget under it is hot, but while one is active, only that one can be.
  void Move(const DrawList& _list, float _xPixels, float _yPixels);
  // A press: the canvas's when it is on a panel of _list (ADR-015 decision 5), when the widget under it that takes a press
  // is active until the release. A press anywhere but the field with the keyboard ends its typing and keeps its text. While
  // a drop-down is open, a press outside its overlay closes it and goes no further.
  [[nodiscard]] bool Press(const DrawList& _list, float _xPixels, float _yPixels);
  // The release, which the canvas never takes, so that it reaches the controls too: the widget clicked, pressed and let go
  // over, if any.
  [[nodiscard]] WidgetId Release(const DrawList& _list, float _xPixels, float _yPixels);
  // The wheel turned _units over a point: the canvas's only over a region that scrolls, whose offset takes it.
  [[nodiscard]] bool Wheel(const DrawList& _list, float _xPixels, float _yPixels, float _units);
  // How far down the pointer has moved since the press, or since this was last taken, in pixels, for a widget dragged
  // while it is active, such as a scroll's thumb.
  [[nodiscard]] float TakeDragPixels() noexcept;

  // _widget, a text field, takes the keyboard.
  void Focus(WidgetId _widget) noexcept;
  // Whether a field has the keyboard, when no order, group or camera key reads one (ADR-017 decision 8).
  [[nodiscard]] bool TakesKeys() const noexcept
  {
    return m_focused != NO_WIDGET;
  }
  // A character, or a key a field reads, for the field with the keyboard; false when none has it. Enter ends its typing.
  [[nodiscard]] bool Type(char32_t _character);
  [[nodiscard]] bool Key(FieldKey _key);
  // Esc, which reaches first the field with the keyboard, which ends its typing and drops what was typed, then an open
  // drop-down, which closes; false when neither takes it, and it goes on to the windows and the controls.
  [[nodiscard]] bool Escape();
  // What reached _widget since it last took it, in the order it came.
  [[nodiscard]] std::vector<FieldStroke> TakeStrokes(WidgetId _widget);

  [[nodiscard]] float ScrollOf(WidgetId _widget) const noexcept;
  void SetScroll(WidgetId _widget, float _offsetUnits);
  [[nodiscard]] std::size_t CaretOf(WidgetId _widget) const noexcept;
  void SetCaret(WidgetId _widget, std::size_t _caret);
  // The drop-down open, of which there is one at most, and whose list stands in an overlay over every window.
  [[nodiscard]] WidgetId DropDown() const noexcept
  {
    return m_dropDown;
  }
  void OpenDropDown(WidgetId _widget) noexcept;
  void CloseDropDown() noexcept;

private:
  // What typing reaches is kept for _widget, dropping what was kept for another.
  void Stroke(WidgetId _widget, FieldStroke _stroke);

  std::vector<WidgetId> m_ids;
  std::unordered_set<WidgetId> m_claimed;
  WidgetId m_hot = NO_WIDGET;
  WidgetId m_active = NO_WIDGET;
  WidgetId m_focused = NO_WIDGET;
  WidgetId m_dropDown = NO_WIDGET;
  // Where the pointer is down the screen, and where a drag was last taken from.
  float m_pointerYPixels = 0.0f;
  float m_dragFromYPixels = 0.0f;
  // The field the strokes are for, which may have given back the keyboard since, as Esc and Enter make it.
  WidgetId m_strokesFor = NO_WIDGET;
  std::vector<FieldStroke> m_strokes;
  std::unordered_map<WidgetId, float> m_scrolls;
  std::unordered_map<WidgetId, std::size_t> m_carets;
};
} // namespace Neuron
