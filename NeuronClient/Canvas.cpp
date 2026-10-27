#include "pch.h"
#include "Canvas.h"

#include <algorithm>

namespace
{
bool SameColor(const DirectX::XMFLOAT4& _first, const DirectX::XMFLOAT4& _second) noexcept
{
  return _first.x == _second.x && _first.y == _second.y && _first.z == _second.z && _first.w == _second.w;
}
} // namespace

Neuron::Canvas::Canvas(const Theme& _theme, const TextMetrics& _metrics, Interaction& _interaction, float _screenWidthUnits,
                       float _screenHeightUnits)
  : m_theme(_theme),
    m_metrics(_metrics),
    m_interaction(_interaction),
    m_tree(_screenWidthUnits, _screenHeightUnits)
{
}

std::size_t Neuron::Canvas::BeginWindow(WidgetId _window, std::string _title, CanvasPoint _corner, float _widthUnits,
                                        const LayoutStyle& _body, float _mostHeightUnits)
{
  return BeginWindow(_window, std::move(_title), LayoutAnchor::TopLeft, {.left = _corner.xUnits, .top = _corner.yUnits}, _widthUnits, _body,
                     _mostHeightUnits);
}

std::size_t Neuron::Canvas::BeginWindow(WidgetId _window, std::string _title, LayoutAnchor _anchor, const LayoutEdges& _margin,
                                        float _widthUnits, const LayoutStyle& _body, float _mostHeightUnits)
{
  const bool claimed = m_interaction.Claim(_window);
  ASSERT(claimed);
  const WidgetId titleBar = TitleBarOf(_window);
  const WidgetId closeBox = CloseBoxOf(_window);
  const bool titleClaimed = m_interaction.Claim(titleBar);
  const bool closeClaimed = m_interaction.Claim(closeBox);
  ASSERT(titleClaimed && closeClaimed);
  // The body stretches across the frame, so that it holds its children within the window's width whatever they would take,
  // and gives up the room the frame lacks under its most.
  const std::size_t frame = m_tree.Begin(
    LayoutKind::Column,
    {.widthUnits = _widthUnits, .mostHeightUnits = _mostHeightUnits, .alignX = LayoutAlign::Stretch, .anchor = _anchor, .margin = _margin});
  (void)m_tree.Add({.heightUnits = m_theme.titleBarUnits});
  LayoutStyle body = _body;
  body.grow = 1.0f;
  (void)m_tree.Begin(LayoutKind::Column, body);
  m_interaction.PushId(_window);
  m_windows.emplace_back(_window, frame);
  m_draws.emplace_back(
    [this, frame, title = std::move(_title)](DrawList& _list) mutable
    {
      const float scale = Scale();
      const CanvasRect body = RectOf(frame, m_theme.windowColor);
      const float titleHeight = m_theme.titleBarUnits * scale;
      const CanvasRect bar{body.left, body.top, body.width - titleHeight, titleHeight, m_theme.titleBarColor, CanvasFill::Hatched};
      const CanvasRect close{body.left + body.width - titleHeight, body.top, titleHeight, titleHeight, m_theme.closeBoxColor};
      _list.BeginLayer(body);
      _list.panels.push_back(body);
      _list.panels.push_back(bar);
      _list.panels.push_back(close);
      _list.texts.push_back({m_theme.closeMark, close.left + (m_theme.closeMarkLeftUnits * scale),
                             body.top + (m_theme.closeMarkTopUnits * scale), m_theme.textColor, m_theme.closeMarkFont});
      if (!title.empty())
        _list.texts.push_back({std::move(title), body.left + (m_theme.titleLeftUnits * scale), body.top + (m_theme.titleTopUnits * scale),
                               m_theme.titleTextColor, m_theme.titleFont, m_theme.titleTrackingUnits * scale});
      Corners(_list, body);
    });
  return frame;
}

void Neuron::Canvas::EndWindow()
{
  ASSERT(!m_windows.empty());
  m_tree.End();
  m_tree.End();
  m_interaction.PopId();
  const WidgetId window = m_windows.back().first;
  const std::size_t frame = m_windows.back().second;
  m_windows.pop_back();
  // The title bar's and the close box's regions after what the window holds, so that a widget on the title bar, such as an
  // arrow, takes a press before the bar does.
  m_draws.emplace_back(
    [this, frame, titleBar = TitleBarOf(window), closeBox = CloseBoxOf(window)](DrawList& _list)
    {
      const CanvasRect body = RectOf(frame, m_theme.windowColor);
      const float titleHeight = m_theme.titleBarUnits * Scale();
      _list.regions.push_back(
        {.area = {body.left, body.top, body.width - titleHeight, titleHeight, m_theme.titleBarColor, CanvasFill::Hatched},
         .widget = titleBar});
      _list.regions.push_back(
        {.area = {body.left + body.width - titleHeight, body.top, titleHeight, titleHeight, m_theme.closeBoxColor}, .widget = closeBox});
    });
}

Neuron::WidgetId Neuron::Canvas::TitleBarOf(WidgetId _window) const noexcept
{
  return Interaction::IdUnder(_window, "title bar");
}

Neuron::WidgetId Neuron::Canvas::CloseBoxOf(WidgetId _window) const noexcept
{
  return Interaction::IdUnder(_window, "close box");
}

std::size_t Neuron::Canvas::BeginPanel(const LayoutStyle& _style)
{
  const std::size_t panel = m_tree.Begin(LayoutKind::Column, _style);
  m_draws.emplace_back(
    [this, panel](DrawList& _list)
    {
      const CanvasRect body = RectOf(panel, m_theme.windowColor);
      _list.panels.push_back(body);
      Corners(_list, body);
    });
  return panel;
}

void Neuron::Canvas::EndPanel()
{
  m_tree.End();
}

std::size_t Neuron::Canvas::BeginBox(const BoxLook& _look, const LayoutStyle& _style)
{
  return OpenBox(NO_WIDGET, _look, NO_PAYLOAD, _style, CardPress::Pressed);
}

Neuron::WidgetId Neuron::Canvas::BeginCard(std::string_view _id, const BoxLook& _look, std::uint32_t _payload, const LayoutStyle& _style,
                                           CardPress _press)
{
  const WidgetId card = Claim(_id);
  (void)OpenBox(card, _look, _payload, _style, _press);
  return card;
}

void Neuron::Canvas::EndBox()
{
  m_tree.End();
}

std::size_t Neuron::Canvas::BeginScroll(std::string_view _id, const LayoutStyle& _style)
{
  const WidgetId view = Claim(_id);
  const WidgetId thumb = Interaction::IdUnder(view, "thumb");
  const bool claimed = m_interaction.Claim(thumb);
  ASSERT(claimed);
  LayoutStyle style = _style;
  style.padding.right += m_theme.scrollLaneUnits;
  style.scrollUnits = m_interaction.ScrollOf(view);
  const std::size_t node = m_tree.Begin(LayoutKind::Scroll, style);
  (void)m_tree.Begin(LayoutKind::Column, {.alignX = LayoutAlign::Stretch});
  auto from = std::make_shared<ListStarts>();
  m_draws.emplace_back(
    [from](DrawList& _list)
    {
      *from = {
        .panels = _list.panels.size(), .texts = _list.texts.size(), .sprites = _list.sprites.size(), .regions = _list.regions.size()};
    });
  m_scrolls.push_back({.view = view, .thumb = thumb, .node = node, .from = std::move(from)});
  return node;
}

void Neuron::Canvas::EndScroll()
{
  ASSERT(!m_scrolls.empty());
  m_tree.End();
  m_tree.End();
  const OpenScroll open = m_scrolls.back();
  m_scrolls.pop_back();
  const bool dragged = m_interaction.Active() == open.thumb;
  m_draws.emplace_back(
    [this, open, dragged](DrawList& _list)
    {
      const LayoutNode& view = m_tree.Node(open.node);
      const LayoutEdges& padding = view.style.padding;
      const float shownUnits = std::max(0.0f, view.heightUnits - padding.top - padding.bottom);
      const float viewLeft = view.leftUnits + padding.left;
      const float viewTop = view.topUnits + padding.top;
      const float viewWidth = std::max(0.0f, view.widthUnits - padding.left - padding.right);
      Cut(_list, *open.from, Area(viewLeft, viewTop, viewWidth, shownUnits, {}));
      _list.regions.push_back({.area = RectOf(open.node, {}), .widget = open.view, .hoverOnly = true, .scrolls = true});
      const float wholeUnits = view.firstChild == LayoutNode::NO_NODE ? 0.0f : m_tree.Node(view.firstChild).heightUnits;
      const float spareUnits = std::max(0.0f, wholeUnits - shownUnits);
      float offset = view.scrolledUnits;
      if (spareUnits > 0.0f && shownUnits > 0.0f)
      {
        // The thumb is as long against its track as the view against what it holds, and stands as far down it as the offset
        // is through what is spare.
        const float thumbUnits = std::min(shownUnits, std::max(m_theme.scrollLeastThumbUnits, shownUnits * shownUnits / wholeUnits));
        const float travel = shownUnits - thumbUnits;
        const float laneLeft = viewLeft + viewWidth + ((m_theme.scrollLaneUnits - m_theme.scrollThumbUnits) / 2.0f);
        _list.panels.push_back(Area(laneLeft, viewTop, m_theme.scrollThumbUnits, shownUnits, m_theme.barTrackColor));
        const CanvasRect thumb =
          Area(laneLeft, viewTop + (travel * offset / spareUnits), m_theme.scrollThumbUnits, thumbUnits, m_theme.barFillColor);
        _list.panels.push_back(thumb);
        _list.regions.push_back({.area = thumb, .widget = open.thumb});
        if (dragged && travel > 0.0f)
          offset = std::clamp(offset + (m_interaction.TakeDragPixels() / Scale() * spareUnits / travel), 0.0f, spareUnits);
      }
      m_interaction.SetScroll(open.view, offset);
    });
}

void Neuron::Canvas::Cut(DrawList& _list, const ListStarts& _from, const CanvasRect& _area) const
{
  const float right = _area.left + _area.width;
  const float bottom = _area.top + _area.height;
  const auto outside = [&_area, right, bottom](float _left, float _top, float _right, float _bottom)
  { return _right <= _area.left || _left >= right || _bottom <= _area.top || _top >= bottom; };
  const auto from = [](auto& _items, std::size_t _first) { return _items.begin() + static_cast<std::ptrdiff_t>(_first); };
  _list.panels.erase(std::remove_if(from(_list.panels, _from.panels), _list.panels.end(), [&outside](const CanvasRect& _panel)
                                    { return outside(_panel.left, _panel.top, _panel.left + _panel.width, _panel.top + _panel.height); }),
                     _list.panels.end());
  _list.texts.erase(std::remove_if(from(_list.texts, _from.texts), _list.texts.end(),
                                   [this, &outside](const CanvasText& _text)
                                   {
                                     const std::span<const GlyphAtlas::Font> fonts = m_metrics.Fonts();
                                     const float line = _text.font < fonts.size() ? fonts[_text.font].lineHeight : 0.0f;
                                     const float width = m_metrics.Width(_text.font, _text.text, _text.trackingPixels / Scale()) * Scale();
                                     return outside(_text.left, _text.top, _text.left + width, _text.top + line);
                                   }),
                    _list.texts.end());
  _list.sprites.erase(std::remove_if(from(_list.sprites, _from.sprites), _list.sprites.end(),
                                     [&outside](const CanvasSprite& _sprite)
                                     {
                                       const CanvasRect& area = _sprite.area;
                                       return outside(area.left, area.top, area.left + area.width, area.top + area.height);
                                     }),
                      _list.sprites.end());
  _list.regions.erase(std::remove_if(from(_list.regions, _from.regions), _list.regions.end(),
                                     [&outside](const CanvasRegion& _region)
                                     {
                                       const CanvasRect& area = _region.area;
                                       return outside(area.left, area.top, area.left + area.width, area.top + area.height);
                                     }),
                      _list.regions.end());
  for (std::size_t region = _from.regions; region < _list.regions.size(); ++region)
  {
    CanvasRect& area = _list.regions[region].area;
    const float left = std::max(area.left, _area.left);
    const float top = std::max(area.top, _area.top);
    area.width = std::min(area.left + area.width, right) - left;
    area.height = std::min(area.top + area.height, bottom) - top;
    area.left = left;
    area.top = top;
  }
  _list.clips.push_back({.area = _area,
                         .panels = {.first = _from.panels, .end = _list.panels.size()},
                         .texts = {.first = _from.texts, .end = _list.texts.size()},
                         .sprites = {.first = _from.sprites, .end = _list.sprites.size()}});
}

std::size_t Neuron::Canvas::Label(LayoutText _text, const DirectX::XMFLOAT4& _color, const LayoutStyle& _style)
{
  const std::size_t label = m_tree.AddText(std::move(_text), _style);
  m_draws.emplace_back([this, label, _color](DrawList& _list) { Lines(_list, label, _color); });
  return label;
}

std::size_t Neuron::Canvas::Icon(std::size_t _sprite, float _sizeUnits, const DirectX::XMFLOAT4& _color, const LayoutStyle& _style)
{
  LayoutStyle style = _style;
  style.widthUnits = style.widthUnits.value_or(_sizeUnits + style.padding.left + style.padding.right);
  style.heightUnits = style.heightUnits.value_or(_sizeUnits + style.padding.top + style.padding.bottom);
  const std::size_t icon = m_tree.Add(style);
  m_draws.emplace_back(
    [this, icon, _sprite, _sizeUnits, _color](DrawList& _list)
    {
      const LayoutNode& node = m_tree.Node(icon);
      _list.sprites.push_back(
        {.sprite = _sprite,
         .area = Area(node.leftUnits + node.style.padding.left, node.topUnits + node.style.padding.top, _sizeUnits, _sizeUnits, _color)});
    });
  return icon;
}

Neuron::WidgetId Neuron::Canvas::Button(std::string_view _id, const ButtonContent& _button, const LayoutStyle& _style)
{
  const WidgetId button = Claim(_id);
  const std::size_t node = m_tree.Add(_style);
  const bool hot = m_interaction.Hot() == button;
  // A copy of its own, which the draw moves with it, where a copy of the reference would be const and copied on each move.
  m_draws.emplace_back([this, node, button, content = _button, hot](DrawList& _list) { DrawButton(_list, node, button, content, hot); });
  return button;
}

Neuron::WidgetId Neuron::Canvas::SmallButton(std::string_view _id, std::string _mark, bool _enabled, std::uint32_t _payload)
{
  const WidgetId button = Claim(_id);
  const std::size_t node = m_tree.Add({.widthUnits = m_theme.smallButtonUnits, .heightUnits = m_theme.smallButtonUnits});
  m_draws.emplace_back([this, node, button, mark = std::move(_mark), _enabled, _payload](DrawList& _list)
                       { DrawSmallButton(_list, node, button, mark, _enabled, _payload); });
  return button;
}

void Neuron::Canvas::Stepper(std::string_view _id, LayoutText _subject, const DirectX::XMFLOAT4& _color, float _roomUnits,
                             float _textTopUnits, bool _canStep, std::uint32_t _previous, std::uint32_t _next)
{
  const float arrows = 2.0f * (m_theme.smallButtonUnits + m_theme.arrowGapUnits);
  _subject.flow = TextFlow::Fit;
  m_interaction.PushId(m_interaction.IdOf(_id));
  (void)m_tree.Begin(LayoutKind::Row, {.gapUnits = m_theme.arrowGapUnits});
  (void)SmallButton("<", "<", _canStep, _previous);
  (void)Label(std::move(_subject), _color, {.mostWidthUnits = _roomUnits - arrows, .padding = {.top = _textTopUnits}});
  (void)SmallButton(">", ">", _canStep, _next);
  m_tree.End();
  m_interaction.PopId();
}

void Neuron::Canvas::Cells(std::string_view _id, std::span<const LayoutText> _words, std::span<const std::uint32_t> _payloads,
                           std::optional<std::size_t> _picked, float _heightUnits, const LayoutStyle& _style)
{
  ASSERT(_words.size() == _payloads.size());
  LayoutStyle grid = _style;
  grid.columns = _words.size();
  grid.alignX = LayoutAlign::Stretch;
  m_interaction.PushId(m_interaction.IdOf(_id));
  (void)m_tree.Begin(LayoutKind::Grid, grid);
  for (std::size_t index = 0; index < _words.size(); ++index)
  {
    const WidgetId cell = m_interaction.IdOf(std::uint64_t{index});
    const bool claimed = m_interaction.Claim(cell);
    ASSERT(claimed);
    const std::size_t node =
      m_tree.Begin(LayoutKind::Stack, {.heightUnits = _heightUnits, .alignX = LayoutAlign::Center, .alignY = LayoutAlign::Center});
    const std::size_t words = m_tree.AddText(_words[index]);
    m_tree.End();
    const bool lit = _picked == index;
    const bool hot = m_interaction.Hot() == cell;
    m_draws.emplace_back(
      [this, node, words, cell, lit, hot, payload = _payloads[index]](DrawList& _list)
      {
        const CanvasRect face = RectOf(node, lit ? m_theme.pickedColor : m_theme.cardColor);
        _list.panels.push_back(face);
        _list.regions.push_back({.area = face, .widget = cell, .payload = payload});
        const LayoutNode& box = m_tree.Node(node);
        Outline(_list, box.leftUnits, box.topUnits, box.widthUnits, box.heightUnits,
                lit   ? m_theme.pickedEdgeColor
                : hot ? m_theme.hoverEdgeColor
                      : m_theme.edgeColor);
        Lines(_list, words, m_theme.textColor);
      });
  }
  m_tree.End();
  m_interaction.PopId();
}

Neuron::WidgetId Neuron::Canvas::Checkbox(std::string_view _id, LayoutText _label, bool _checked, std::uint32_t _payload)
{
  const WidgetId box = Claim(_id);
  (void)m_tree.Begin(LayoutKind::Row, {.gapUnits = m_theme.checkGapUnits, .alignY = LayoutAlign::Center});
  const std::size_t square = m_tree.Add({.widthUnits = m_theme.checkUnits, .heightUnits = m_theme.checkUnits});
  const std::size_t label = m_tree.AddText(std::move(_label));
  m_tree.End();
  const bool hot = m_interaction.Hot() == box;
  m_draws.emplace_back(
    [this, square, label, box, _checked, _payload, hot](DrawList& _list)
    {
      const LayoutNode& node = m_tree.Node(square);
      const CanvasRect face = RectOf(square, m_theme.fieldColor);
      _list.panels.push_back(face);
      _list.regions.push_back({.area = face, .widget = box, .payload = _payload});
      Outline(_list, node.leftUnits, node.topUnits, node.widthUnits, node.heightUnits, hot ? m_theme.hoverEdgeColor : m_theme.edgeColor);
      if (_checked)
      {
        const float inset = m_theme.checkInsetUnits;
        _list.sprites.push_back({.sprite = m_theme.checkSprite,
                                 .area = Area(node.leftUnits + inset, node.topUnits + inset, node.widthUnits - (2.0f * inset),
                                              node.heightUnits - (2.0f * inset), m_theme.textColor)});
      }
      Lines(_list, label, m_theme.textColor);
    });
  return box;
}

std::size_t Neuron::Canvas::ProgressBar(float _share, const LayoutStyle& _style)
{
  const std::size_t bar = m_tree.Add(_style);
  m_draws.emplace_back(
    [this, bar, share = std::clamp(_share, 0.0f, 1.0f)](DrawList& _list)
    {
      const LayoutNode& node = m_tree.Node(bar);
      _list.panels.push_back(RectOf(bar, m_theme.barTrackColor));
      _list.panels.push_back(Area(node.leftUnits, node.topUnits, node.widthUnits * share, node.heightUnits, m_theme.barFillColor));
    });
  return bar;
}

std::size_t Neuron::Canvas::Chip(LayoutText _text)
{
  const float widthUnits = m_metrics.Width(_text.font, _text.text, _text.trackingUnits) + (2.0f * m_theme.chipPadUnits);
  const std::size_t chip = m_tree.Add({.widthUnits = widthUnits, .heightUnits = _text.lineUnits});
  m_draws.emplace_back(
    [this, chip, text = std::move(_text)](DrawList& _list) mutable
    {
      const LayoutNode& node = m_tree.Node(chip);
      _list.panels.push_back(RectOf(chip, m_theme.warningColor));
      Text(_list, std::move(text.text), node.leftUnits + m_theme.chipPadUnits, node.topUnits, m_theme.windowColor, text.font,
           text.trackingUnits);
    });
  return chip;
}

std::size_t Neuron::Canvas::KeyCap(std::string _key)
{
  const std::size_t cap = m_tree.Add({.widthUnits = m_theme.keyCapUnits, .heightUnits = m_theme.keyCapUnits});
  m_draws.emplace_back(
    [this, cap, key = std::move(_key)](DrawList& _list)
    {
      const LayoutNode& node = m_tree.Node(cap);
      Cap(_list, key, node.leftUnits, node.topUnits);
    });
  return cap;
}

Neuron::FieldEnd Neuron::Canvas::TextField(std::string_view _id, std::string& _text, std::size_t _limitCharacters, bool _valid,
                                           const LayoutStyle& _style)
{
  const WidgetId field = Claim(_id);
  if (m_interaction.Active() == field)
    m_interaction.Focus(field);
  FieldEnd end = FieldEnd::Typing;
  for (const FieldStroke& stroke : m_interaction.TakeStrokes(field))
  {
    if (stroke.key == FieldKey::Character)
    {
      // Only what the atlas's fonts hold, which is a byte of UTF-8 each (ADR-030).
      if (stroke.character >= GlyphAtlas::FIRST && stroke.character <= GlyphAtlas::LAST && _text.size() < _limitCharacters)
        _text.push_back(static_cast<char>(stroke.character));
    }
    else if (stroke.key == FieldKey::Backspace)
    {
      if (!_text.empty())
        _text.pop_back();
    }
    else
      end = stroke.key == FieldKey::Enter ? FieldEnd::Entered : FieldEnd::Dropped;
  }
  const bool focused = m_interaction.Focused() == field;
  const std::size_t node = m_tree.Add(_style);
  m_draws.emplace_back(
    [this, node, field, focused, _valid, shown = focused ? _text + "_" : _text](DrawList& _list)
    {
      const LayoutNode& box = m_tree.Node(node);
      const LayoutEdges& padding = box.style.padding;
      const CanvasRect face = RectOf(node, m_theme.fieldColor);
      _list.panels.push_back(face);
      _list.regions.push_back({.area = face, .widget = field});
      Outline(_list, box.leftUnits, box.topUnits, box.widthUnits, box.heightUnits, focused ? m_theme.pickedEdgeColor : m_theme.edgeColor);
      const float contentHeight = box.heightUnits - padding.top - padding.bottom;
      Text(_list, m_metrics.Fit(m_theme.nameFont, shown, box.widthUnits - padding.left - padding.right - (2.0f * m_theme.buttonInsetUnits)),
           box.leftUnits + padding.left + m_theme.buttonInsetUnits,
           box.topUnits + padding.top + ((contentHeight - m_theme.nameLineUnits) / 2.0f), _valid ? m_theme.textColor : m_theme.warningColor,
           m_theme.nameFont);
    });
  return end;
}

// A box's face, its hatch over it, the region a card takes a press in or answers the pointer in, and its edge, in the order
// the HUD's helpers draw a card.
std::size_t Neuron::Canvas::OpenBox(WidgetId _widget, const BoxLook& _look, std::uint32_t _payload, const LayoutStyle& _style,
                                    CardPress _press)
{
  ASSERT(MayFill(m_theme, _look.face) && MayFill(m_theme, _look.edge) && (!_look.hatch.has_value() || MayFill(m_theme, *_look.hatch)));
  const std::size_t box = m_tree.Begin(LayoutKind::Column, _style);
  m_draws.emplace_back(
    [this, box, _widget, look = _look, _payload, _press](DrawList& _list)
    {
      const LayoutNode& node = m_tree.Node(box);
      const CanvasRect face = RectOf(box, look.face);
      _list.panels.push_back(face);
      if (look.hatch.has_value())
        _list.panels.push_back(RectOf(box, *look.hatch, CanvasFill::Hatched));
      if (_payload != NO_PAYLOAD)
        _list.regions.push_back({.area = face, .widget = _widget, .payload = _payload, .hoverOnly = _press == CardPress::Previewed});
      Outline(_list, node.leftUnits, node.topUnits, node.widthUnits, node.heightUnits, look.edge);
    });
  return box;
}

void Neuron::Canvas::Fill(std::size_t _node, const DirectX::XMFLOAT4& _color, CanvasFill _fill)
{
  ASSERT(MayFill(m_theme, _color));
  m_draws.emplace_back([this, _node, _color, _fill](DrawList& _list) { _list.panels.push_back(RectOf(_node, _color, _fill)); });
}

bool Neuron::Canvas::MayFill(const Theme& _theme, const DirectX::XMFLOAT4& _color) noexcept
{
  return !SameColor(_color, _theme.warningColor);
}

void Neuron::Canvas::Finish(DrawList& _list)
{
  ASSERT(m_windows.empty() && m_scrolls.empty());
  m_tree.Arrange(m_metrics);
  for (Draw& draw : m_draws)
    draw(_list);
}

Neuron::WidgetId Neuron::Canvas::Claim(std::string_view _id)
{
  const WidgetId widget = m_interaction.IdOf(_id);
  const bool claimed = m_interaction.Claim(widget);
  ASSERT(claimed);
  return widget;
}

Neuron::CanvasRect Neuron::Canvas::RectOf(std::size_t _node, const DirectX::XMFLOAT4& _color, CanvasFill _fill) const
{
  const LayoutNode& node = m_tree.Node(_node);
  return Area(node.leftUnits, node.topUnits, node.widthUnits, node.heightUnits, _color, _fill);
}

Neuron::CanvasRect Neuron::Canvas::Area(float _leftUnits, float _topUnits, float _widthUnits, float _heightUnits,
                                        const DirectX::XMFLOAT4& _color, CanvasFill _fill) const noexcept
{
  const float scale = Scale();
  return {_leftUnits * scale, _topUnits * scale, _widthUnits * scale, _heightUnits * scale, _color, _fill};
}

void Neuron::Canvas::Outline(DrawList& _list, float _leftUnits, float _topUnits, float _widthUnits, float _heightUnits,
                             const DirectX::XMFLOAT4& _color) const
{
  const float edge = m_theme.edgeUnits;
  _list.panels.push_back(Area(_leftUnits, _topUnits, _widthUnits, edge, _color));
  _list.panels.push_back(Area(_leftUnits, _topUnits + _heightUnits - edge, _widthUnits, edge, _color));
  _list.panels.push_back(Area(_leftUnits, _topUnits, edge, _heightUnits, _color));
  _list.panels.push_back(Area(_leftUnits + _widthUnits - edge, _topUnits, edge, _heightUnits, _color));
}

void Neuron::Canvas::Text(DrawList& _list, std::string _text, float _leftUnits, float _topUnits, const DirectX::XMFLOAT4& _color,
                          std::size_t _font, float _trackingUnits) const
{
  const float scale = Scale();
  _list.texts.push_back({std::move(_text), _leftUnits * scale, _topUnits * scale, _color, _font, _trackingUnits * scale});
}

void Neuron::Canvas::Lines(DrawList& _list, std::size_t _node, const DirectX::XMFLOAT4& _color) const
{
  const LayoutNode& node = m_tree.Node(_node);
  for (const LayoutLine& line : node.lines)
    Text(_list, line.text, line.leftUnits, line.topUnits, _color, node.text.font, node.text.trackingUnits);
}

void Neuron::Canvas::Corners(DrawList& _list, const CanvasRect& _frame) const
{
  const float corner = m_theme.cornerUnits * Scale();
  for (const bool right : {false, true})
  {
    for (const bool bottom : {false, true})
    {
      _list.sprites.push_back({.sprite = m_theme.cornerSprite,
                               .area = {right ? _frame.left + _frame.width - corner : _frame.left,
                                        bottom ? _frame.top + _frame.height - corner : _frame.top, corner, corner, m_theme.cornerColor},
                               .mirrorX = right,
                               .mirrorY = bottom});
    }
  }
}

void Neuron::Canvas::Cap(DrawList& _list, const std::string& _key, float _leftUnits, float _topUnits) const
{
  const float side = m_theme.keyCapUnits;
  _list.panels.push_back(Area(_leftUnits, _topUnits, side, side, m_theme.fieldColor));
  Outline(_list, _leftUnits, _topUnits, side, side, m_theme.keyColor);
  Text(_list, _key, _leftUnits + ((side - m_metrics.Width(m_theme.labelFont, _key)) / 2.0f), _topUnits + m_theme.keyCapTextTopUnits,
       m_theme.keyColor, m_theme.labelFont);
}

void Neuron::Canvas::DrawButton(DrawList& _list, std::size_t _node, WidgetId _widget, const ButtonContent& _button, bool _hot) const
{
  const LayoutNode& node = m_tree.Node(_node);
  const float left = node.leftUnits;
  const float top = node.topUnits;
  const float width = node.widthUnits;
  const float height = node.heightUnits;
  const Theme& theme = m_theme;
  const CanvasRect face = Area(left, top, width, height,
                               _button.picked    ? theme.pickedColor
                               : _button.enabled ? theme.cardColor
                                                 : theme.fieldColor);
  _list.panels.push_back(face);
  if (_button.enabled)
    _list.regions.push_back({.area = face, .widget = _widget, .payload = _button.payload});
  Outline(_list, left, top, width, height, _button.picked ? theme.pickedEdgeColor : _hot ? theme.hoverEdgeColor : theme.edgeColor);
  if (_button.progress.has_value())
  {
    const float barWidth = width - (2.0f * theme.barInsetUnits);
    const float barTop = top + height - theme.barInsetUnits - theme.barUnits;
    _list.panels.push_back(Area(left + theme.barInsetUnits, barTop, barWidth, theme.barUnits, theme.barTrackColor));
    _list.panels.push_back(
      Area(left + theme.barInsetUnits, barTop, barWidth * std::clamp(*_button.progress, 0.0f, 1.0f), theme.barUnits, theme.barFillColor));
  }
  const DirectX::XMFLOAT4& labelColor = _button.enabled ? theme.textColor : theme.lockedTextColor;
  const float labelTop = top + ((height - theme.nameLineUnits) / 2.0f);
  const float figureTop = top + ((height - theme.figureLineUnits) / 2.0f);
  const float right = left + width - theme.buttonInsetUnits;
  float room = width - (2.0f * theme.buttonInsetUnits);
  if (!_button.key.empty())
  {
    Cap(_list, _button.key, right - theme.keyCapUnits, top + ((height - theme.keyCapUnits) / 2.0f));
    room -= theme.keyCapUnits + theme.fitGapUnits;
  }
  if (!_button.enabled && !_button.note.empty())
  {
    // The reason it cannot be pressed stands in its figure's place, or where one would stand on a button without one.
    const float noteWidth = m_metrics.Width(theme.labelFont, _button.note);
    Text(_list, _button.note, right - noteWidth, figureTop, theme.labelColor, theme.labelFont);
    Text(_list, m_metrics.Fit(theme.nameFont, _button.label, room - noteWidth - theme.fitGapUnits), left + theme.buttonInsetUnits, labelTop,
         labelColor, theme.nameFont);
    return;
  }
  if (_button.figure.empty())
  {
    Text(_list, m_metrics.Fit(theme.nameFont, _button.label, room), left + theme.buttonInsetUnits, labelTop, labelColor, theme.nameFont);
    return;
  }
  const float mark = _button.figureMark.has_value() ? theme.figureMarkUnits : 0.0f;
  const float markGap = _button.figureMark.has_value() ? theme.figureMarkGapUnits : 0.0f;
  const float figureWidth = mark + markGap + m_metrics.Width(theme.figureFont, _button.figure);
  Text(_list, m_metrics.Fit(theme.nameFont, _button.label, room - figureWidth - theme.fitGapUnits), left + theme.buttonInsetUnits, labelTop,
       labelColor, theme.nameFont);
  const DirectX::XMFLOAT4& figureColor = _button.enabled ? theme.figureColor : theme.lockedTextColor;
  const float figureLeft = right - figureWidth;
  if (_button.figureMark.has_value())
    _list.sprites.push_back(
      {.sprite = *_button.figureMark, .area = Area(figureLeft, figureTop + theme.figureMarkTopUnits, mark, mark, figureColor)});
  Text(_list, _button.figure, figureLeft + mark + markGap, figureTop, figureColor, theme.figureFont);
}

void Neuron::Canvas::DrawSmallButton(DrawList& _list, std::size_t _node, WidgetId _widget, const std::string& _mark, bool _enabled,
                                     std::uint32_t _payload) const
{
  const LayoutNode& node = m_tree.Node(_node);
  const float side = m_theme.smallButtonUnits;
  const CanvasRect face = Area(node.leftUnits, node.topUnits, side, side, _enabled ? m_theme.cardColor : m_theme.disabledColor);
  _list.panels.push_back(face);
  if (_enabled)
    _list.regions.push_back({.area = face, .widget = _widget, .payload = _payload});
  Text(_list, _mark, node.leftUnits + ((side - m_metrics.Width(m_theme.labelFont, _mark)) / 2.0f),
       node.topUnits + m_theme.smallButtonTextTopUnits, _enabled ? m_theme.textColor : m_theme.dimTextColor, m_theme.labelFont);
}
