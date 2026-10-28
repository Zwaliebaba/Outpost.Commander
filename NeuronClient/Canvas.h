#pragma once

namespace Neuron
{
// What a button shows (ADR-043 decision 4): its label; a figure, such as its cost, after the mark it may have, at its
// right; why it cannot be pressed, in the figure's place while it is disabled; its key, as a cap at its right; and work it
// started that is under way, from nothing to one, as a bar along its foot. Stacked, the figure or the reason stands under
// the label instead, both from the button's left, for a button too narrow for the two on one line, such as a structure's
// on the command card (ADR-088 decision 2). A press on it, while it is enabled, names the caller's payload. Its edge is lit
// under the pointer, or where the caller says it is pointed at, as the HUD lights each button whose action is the one
// under the pointer (ADR-088 decision 1).
struct ButtonContent
{
  std::string label;
  std::string figure;
  std::optional<std::size_t> figureMark;
  std::string note;
  std::string key;
  std::uint32_t payload = NO_PAYLOAD;
  bool enabled = true;
  bool picked = false;
  std::optional<float> progress;
  bool pointed = false;
  bool stacked = false;
};

// How a box the game composes looks (ADR-100): its face, a hatch over its face if it has one, and its edge, each any color
// but the warning's.
struct BoxLook
{
  DirectX::XMFLOAT4 face{};
  std::optional<DirectX::XMFLOAT4> hatch;
  DirectX::XMFLOAT4 edge{};
};

// What a card does under the pointer: takes a press naming its payload, or only answers the pointer with it and takes no
// press, as a part's card research has yet to unlock previews what it buys (ADR-017 decision 7).
enum class CardPress : std::uint8_t
{
  Pressed,
  Previewed
};

// What a text field did with what reached it this frame (ADR-017 decision 8): nothing more than edit its text, or end
// typing, by Enter, which keeps the text, or by Esc, which drops what was typed.
enum class FieldEnd : std::uint8_t
{
  Typing,
  Entered,
  Dropped
};

// The canvas a frame's interface is built on (ADR-100): widgets in the theme's look, each a node of the layout and what it
// draws there, and a window, a layer of its own. The build records them in the order they are drawn: the HUD's first,
// then each window back to front. Finish measures and places the layout and draws what was recorded into a display
// list, each place in reference units times the metrics' scale. A widget's identifier is its label's under the window
// it is in; two in one frame are a failed assertion (ADR-093).
class Canvas
{
public:
  // The theme, the metrics and the interaction must outlive the canvas.
  Canvas(const Theme& _theme, const TextMetrics& _metrics, Interaction& _interaction, float _screenWidthUnits, float _screenHeightUnits);

  // The layout the widgets are nodes of, for columns, rows, grids, stacks and spacers.
  [[nodiscard]] LayoutTree& Tree() noexcept
  {
    return m_tree;
  }

  // A floating window (ADR-031 decision 3): a layer of its own, its frame with its top-left corner at _corner, _widthUnits
  // wide and as tall as its title bar and its body, but no taller than _mostHeightUnits while a scroll view in it can give
  // up room; its hatched title bar, with its close box's mark and _title, and its close box; and a bracket at each corner.
  // Its body is a column of _body's style, which takes what is built until EndWindow, named under _window. The frame's
  // node, for where it stood once the canvas is finished.
  std::size_t BeginWindow(WidgetId _window, std::string _title, CanvasPoint _corner, float _widthUnits, const LayoutStyle& _body = {},
                          float _mostHeightUnits = UNBOUNDED_UNITS);
  // A window that stands at _anchor of the screen, less _margin, where it first opens before it is moved.
  std::size_t BeginWindow(WidgetId _window, std::string _title, LayoutAnchor _anchor, const LayoutEdges& _margin, float _widthUnits,
                          const LayoutStyle& _body = {}, float _mostHeightUnits = UNBOUNDED_UNITS);
  void EndWindow();
  // The identifiers of a window's title bar, which a press drags it by, and of its close box.
  [[nodiscard]] WidgetId TitleBarOf(WidgetId _window) const noexcept;
  [[nodiscard]] WidgetId CloseBoxOf(WidgetId _window) const noexcept;

  // A panel of the HUD, in a window's look though it has no title bar (ADR-043 decision 1): a column of _style, its body
  // and a bracket at each corner, which takes what is built until EndPanel.
  std::size_t BeginPanel(const LayoutStyle& _style);
  void EndPanel();

  // A box of _style in _look, a column that takes what is built until EndBox, for what the game composes: a frame, such as
  // a queue's row or the Ore's box, or with BeginCard a card the player presses, such as a production option or a topic,
  // which takes a press naming _payload unless that is NO_PAYLOAD, or as _press has it, only answers the pointer with it. A
  // card's edge is not lit under the pointer.
  std::size_t BeginBox(const BoxLook& _look, const LayoutStyle& _style);
  WidgetId BeginCard(std::string_view _id, const BoxLook& _look, std::uint32_t _payload, const LayoutStyle& _style,
                     CardPress _press = CardPress::Pressed);
  void EndBox();

  // A view of what is built until EndScroll, of _style, that scrolls it when it is the taller (ADR-100): what it holds as
  // tall as that is, cut to the view, and moved by the wheel over the view and by its thumb, dragged, which stands with its
  // track in a lane at the view's right while there is more than the view shows. The view gives up room down to its least
  // when the window lacks it, when its style grows.
  std::size_t BeginScroll(std::string_view _id, const LayoutStyle& _style);
  void EndScroll();

  // A text in _color: one line, one cut short, or wrapped, as _text flows (ADR-061 decision 3).
  std::size_t Label(LayoutText _text, const DirectX::XMFLOAT4& _color, const LayoutStyle& _style = {});
  // The atlas's sprite of index _sprite, _sizeUnits square, in _color, at the top-left of its node's content box, such as
  // Ore's gem before a figure. The node is as large as the sprite and its padding unless _style gives it a size.
  std::size_t Icon(std::size_t _sprite, float _sizeUnits, const DirectX::XMFLOAT4& _color, const LayoutStyle& _style = {});
  // A button, a card, as _button has it (ADR-043 decision 4): dim and taking no press while disabled, its edge lit under
  // the pointer unless it is picked, whose edge is lit already. The label is cut short only where it would meet the figure,
  // the reason or the cap.
  WidgetId Button(std::string_view _id, const ButtonContent& _button, const LayoutStyle& _style);
  // A small square button with _mark in its middle, such as an arrow.
  WidgetId SmallButton(std::string_view _id, std::string _mark, bool _enabled, std::uint32_t _payload);
  // _subject between the arrows that step it, "< SHIPYARD 05 >" (ADR-068 decision 1): cut short to leave the arrows room
  // within _roomUnits, its line _textTopUnits under the arrows' top. The arrows name _previous and _next, while _canStep.
  void Stepper(std::string_view _id, LayoutText _subject, const DirectX::XMFLOAT4& _color, float _roomUnits, float _textTopUnits,
               bool _canStep, std::uint32_t _previous, std::uint32_t _next);
  // A row of cells, a grid of _style one row long, each cell _heightUnits tall with its words in its middle, the one at
  // _picked lit, and each naming its payload (ADR-088 decision 3).
  void Cells(std::string_view _id, std::span<const LayoutText> _words, std::span<const std::uint32_t> _payloads,
             std::optional<std::size_t> _picked, float _heightUnits, const LayoutStyle& _style);
  // A box with a tick while _checked, and _label after it; a press on the box names _payload.
  WidgetId Checkbox(std::string_view _id, LayoutText _label, bool _checked, std::uint32_t _payload);
  // A bar of _style's size, filled from its left by _share, from nothing to one.
  std::size_t ProgressBar(float _share, const LayoutStyle& _style);
  // A warning (ADR-085 decision 1): its words in the window's color on a tag of the warning's, which reaches the theme's pad
  // beyond them on either side and covers their whole line. Nothing else is filled in the warning's color.
  std::size_t Chip(LayoutText _text);
  // A key's letter on a cap (ADR-068 decision 4).
  std::size_t KeyCap(std::string _key);
  // A field _text is typed into (ADR-017 decision 8): a press on it gives it the keyboard, and while it has it, every
  // character the atlas holds, up to _limitCharacters, and Backspace edit _text, Enter ends typing and keeps it, and Esc
  // ends it and drops what was typed, which the caller puts back. It shows a caret while it has the keyboard, and its text
  // in the warning's color while it is not _valid. Its text stands in its content box, the theme's inset in from its sides
  // and in its middle down, so that its padding keeps room for what the caller sets over it, such as a label and a count.
  FieldEnd TextField(std::string_view _id, std::string& _text, std::size_t _limitCharacters, bool _valid, const LayoutStyle& _style);
  // A drop-down: a field that shows _value, cut short to leave room for its mark at its right, which a press opens and a
  // press on it again closes. Its list of _choices, _picked lit, then stands in an overlay over everything else, as wide as
  // the field and a row as tall as the field for each choice: under the field, or over it where the screen ends under it,
  // and where it is taller than the room either way, on the side with more, scrolled by the wheel and its thumb. It opens
  // with the pick at its top, as far as it goes. A press on a choice names the payload _payloadOf gives it and closes the
  // list; Esc or a press anywhere else closes it and goes no further. The field's node.
  std::size_t DropDown(std::string_view _id, std::string _value, std::vector<std::string> _choices, std::optional<std::size_t> _picked,
                       const std::function<std::uint32_t(std::size_t)>& _payloadOf, const LayoutStyle& _style);

  // A rectangle of _color at a node, for what the game composes of its own, such as a part's card. It may be any color but
  // the warning's, which only a chip fills.
  void Fill(std::size_t _node, const DirectX::XMFLOAT4& _color, CanvasFill _fill = CanvasFill::Solid);
  // A press at a node, naming _payload, which draws nothing, for what the game composes of its own, such as a line of a
  // panel that takes a click.
  void Press(std::size_t _node, std::uint32_t _payload);
  [[nodiscard]] static bool MayFill(const Theme& _theme, const DirectX::XMFLOAT4& _color) noexcept;

  // Measures and places what was built, and draws it into _list, in the order it was built; once, when the build is done.
  void Finish(DrawList& _list);

private:
  using Draw = std::function<void(DrawList&)>;

  // Names _id under the window it is in and claims it for this frame.
  [[nodiscard]] WidgetId Claim(std::string_view _id);
  [[nodiscard]] float Scale() const noexcept
  {
    return m_metrics.Scale();
  }
  [[nodiscard]] CanvasRect RectOf(std::size_t _node, const DirectX::XMFLOAT4& _color, CanvasFill _fill = CanvasFill::Solid) const;
  [[nodiscard]] CanvasRect Area(float _leftUnits, float _topUnits, float _widthUnits, float _heightUnits, const DirectX::XMFLOAT4& _color,
                                CanvasFill _fill = CanvasFill::Solid) const noexcept;
  void Outline(DrawList& _list, float _leftUnits, float _topUnits, float _widthUnits, float _heightUnits,
               const DirectX::XMFLOAT4& _color) const;
  void Text(DrawList& _list, std::string _text, float _leftUnits, float _topUnits, const DirectX::XMFLOAT4& _color, std::size_t _font,
            float _trackingUnits = 0.0f) const;
  void Lines(DrawList& _list, std::size_t _node, const DirectX::XMFLOAT4& _color) const;
  void Corners(DrawList& _list, const CanvasRect& _frame) const;
  void Cap(DrawList& _list, const std::string& _key, float _leftUnits, float _topUnits) const;
  void DrawButton(DrawList& _list, std::size_t _node, WidgetId _widget, const ButtonContent& _button, bool _hot) const;
  void DrawSmallButton(DrawList& _list, std::size_t _node, WidgetId _widget, const std::string& _mark, bool _enabled,
                       std::uint32_t _payload) const;
  std::size_t OpenBox(WidgetId _widget, const BoxLook& _look, std::uint32_t _payload, const LayoutStyle& _style, CardPress _press);
  // Where what a scroll view holds starts in each list, once drawn.
  struct ListStarts
  {
    std::size_t panels = 0;
    std::size_t texts = 0;
    std::size_t sprites = 0;
    std::size_t regions = 0;
  };
  // What a scroll view holds, from _from: what lies wholly outside _area goes, a region is cut to it, and what is left is
  // drawn cut to it.
  void Cut(DrawList& _list, const ListStarts& _from, const CanvasRect& _area) const;
  // Where each list ends now, which is where what is drawn next starts.
  [[nodiscard]] static ListStarts EndsOf(const DrawList& _list) noexcept;
  // The track and the thumb of a view _shownUnits tall of what is _wholeUnits tall, in a lane at _laneLeftUnits from
  // _topUnits, and the thumb's region, while it shows less than all; and _offsetUnits held within what is spare, moved by
  // the thumb's drag while it is _dragged.
  [[nodiscard]] float ScrollBar(DrawList& _list, WidgetId _thumb, float _laneLeftUnits, float _topUnits, float _shownUnits,
                                float _wholeUnits, float _offsetUnits, bool _dragged) const;

  const Theme& m_theme;
  const TextMetrics& m_metrics;
  Interaction& m_interaction;
  LayoutTree m_tree;
  float m_screenHeightUnits = 0.0f;
  std::vector<Draw> m_draws;
  // What is drawn once everything else is, over it: an open drop-down's list.
  std::vector<Draw> m_overlays;
  // The windows open in the build, the innermost last, and their frames' nodes.
  std::vector<std::pair<WidgetId, std::size_t>> m_windows;
  // A scroll view open in the build: its identifier and its thumb's, its node, and where what it holds starts in each list,
  // once drawn.
  struct OpenScroll
  {
    WidgetId view = NO_WIDGET;
    WidgetId thumb = NO_WIDGET;
    std::size_t node = 0;
    std::shared_ptr<ListStarts> from;
  };
  std::vector<OpenScroll> m_scrolls;
};
} // namespace Neuron
