#include "pch.h"
#include "LayoutTree.h"

#include <algorithm>

namespace
{
using Neuron::LayoutAlign;
using Neuron::LayoutKind;
using Neuron::LayoutNode;

float Held(float _units, float _leastUnits, float _mostUnits) noexcept
{
  return std::max(_leastUnits, std::min(_units, _mostUnits));
}

// How far into its room a thing sits that leaves _spareUnits of it, as _align puts it.
float Offset(LayoutAlign _align, float _spareUnits) noexcept
{
  if (_align == LayoutAlign::Center)
    return _spareUnits / 2.0f;
  return _align == LayoutAlign::End ? _spareUnits : 0.0f;
}

float Gaps(std::size_t _count, float _gapUnits) noexcept
{
  return _count > 1 ? _gapUnits * static_cast<float>(_count - 1) : 0.0f;
}

// Where an anchor stands across and down: the anchors are in reading order, three to a row.
LayoutAlign AnchorX(Neuron::LayoutAnchor _anchor) noexcept
{
  constexpr std::array<LayoutAlign, 3> THIRDS{LayoutAlign::Start, LayoutAlign::Center, LayoutAlign::End};
  return THIRDS[static_cast<std::size_t>(_anchor) % THIRDS.size()];
}

LayoutAlign AnchorY(Neuron::LayoutAnchor _anchor) noexcept
{
  constexpr std::array<LayoutAlign, 3> THIRDS{LayoutAlign::Start, LayoutAlign::Center, LayoutAlign::End};
  return THIRDS[static_cast<std::size_t>(_anchor) / THIRDS.size()];
}

// The children of _node in its flow, or those anchored out of it, in the order they were added.
std::vector<std::size_t> ChildrenOf(const std::vector<LayoutNode>& _nodes, const LayoutNode& _node, bool _anchored)
{
  std::vector<std::size_t> children;
  for (std::size_t child = _node.firstChild; child != LayoutNode::NO_NODE; child = _nodes[child].nextSibling)
  {
    if (_nodes[child].style.anchor.has_value() == _anchored)
      children.push_back(child);
  }
  return children;
}

// A child's width in _roomUnits as _align puts it: the width it was given, all the room when stretched, and otherwise
// what it would take, as far as the room goes; held between its least and its most.
float Across(const LayoutNode& _child, float _roomUnits, LayoutAlign _align) noexcept
{
  if (_child.style.widthUnits.has_value())
    return *_child.style.widthUnits;
  const float wanted = _align == LayoutAlign::Stretch ? _roomUnits : std::min(_child.naturalWidthUnits, _roomUnits);
  return Held(wanted, _child.style.leastWidthUnits, _child.style.mostWidthUnits);
}

// A child's height in _roomUnits as _align puts it: the height it was given, all the room when stretched, and otherwise
// what it takes.
float Down(const LayoutNode& _child, float _roomUnits, LayoutAlign _align) noexcept
{
  if (_child.style.heightUnits.has_value())
    return *_child.style.heightUnits;
  if (_align == LayoutAlign::Stretch)
    return Held(_roomUnits, _child.style.leastHeightUnits, _child.style.mostHeightUnits);
  return _child.naturalHeightUnits;
}

// A grid's columns, of which it has at least one.
std::size_t ColumnsOf(const Neuron::LayoutStyle& _style) noexcept
{
  return std::max<std::size_t>(_style.columns, 1);
}

// How tall each of a grid's rows is: as tall as its tallest cell.
std::vector<float> RowHeights(const std::vector<LayoutNode>& _nodes, const std::vector<std::size_t>& _cells, std::size_t _columns)
{
  std::vector<float> rows((_cells.size() + _columns - 1) / _columns, 0.0f);
  for (std::size_t cell = 0; cell < _cells.size(); ++cell)
    rows[cell / _columns] = std::max(rows[cell / _columns], _nodes[_cells[cell]].naturalHeightUnits);
  return rows;
}

// A child's share of a column's or a row's length: what it takes before the room left is shared, its grow, and the least
// and the most it is held between.
struct Share
{
  float baseUnits = 0.0f;
  float grow = 0.0f;
  float leastUnits = 0.0f;
  float mostUnits = Neuron::UNBOUNDED_UNITS;
  float units = 0.0f;
};

// Shares out the room left in _roomUnits, or the room lacked, among the shares that grow, in proportion to their grow. A
// share held at its least or its most keeps that, and what it could not take is shared again among the rest.
void ShareOut(std::vector<Share>& _shares, float _roomUnits)
{
  std::vector<bool> settled(_shares.size());
  for (std::size_t share = 0; share < _shares.size(); ++share)
  {
    _shares[share].units = _shares[share].baseUnits;
    settled[share] = _shares[share].grow <= 0.0f;
  }
  // Each round settles at least one more share, or is the last.
  for (std::size_t round = 0; round < _shares.size(); ++round)
  {
    float spare = _roomUnits;
    float growth = 0.0f;
    for (std::size_t share = 0; share < _shares.size(); ++share)
    {
      spare -= settled[share] ? _shares[share].units : _shares[share].baseUnits;
      growth += settled[share] ? 0.0f : _shares[share].grow;
    }
    if (growth <= 0.0f)
      return;
    bool held = false;
    for (std::size_t share = 0; share < _shares.size(); ++share)
    {
      if (settled[share])
        continue;
      Share& open = _shares[share];
      const float wanted = open.baseUnits + (spare * open.grow / growth);
      open.units = Held(wanted, open.leastUnits, open.mostUnits);
      if (wanted < open.leastUnits || wanted > open.mostUnits)
      {
        settled[share] = true;
        held = true;
      }
    }
    if (!held)
      return;
  }
}
} // namespace

Neuron::LayoutTree::LayoutTree(float _screenWidthUnits, float _screenHeightUnits)
{
  m_open.push_back(Push(LayoutKind::Stack, {.widthUnits = _screenWidthUnits, .heightUnits = _screenHeightUnits}));
}

std::size_t Neuron::LayoutTree::Begin(LayoutKind _kind, const LayoutStyle& _style)
{
  ASSERT(_kind != LayoutKind::Box && _kind != LayoutKind::Text);
  const std::size_t node = Push(_kind, _style);
  m_open.push_back(node);
  return node;
}

void Neuron::LayoutTree::End()
{
  ASSERT(m_open.size() > 1);
  m_open.pop_back();
}

std::size_t Neuron::LayoutTree::Add(const LayoutStyle& _style)
{
  return Push(LayoutKind::Box, _style);
}

std::size_t Neuron::LayoutTree::AddSpacer(float _grow)
{
  return Push(LayoutKind::Box, {.grow = _grow});
}

std::size_t Neuron::LayoutTree::AddText(LayoutText _text, const LayoutStyle& _style)
{
  const std::size_t node = Push(LayoutKind::Text, _style);
  m_nodes[node].text = std::move(_text);
  return node;
}

void Neuron::LayoutTree::Arrange(const TextMetrics& _metrics)
{
  ASSERT(m_open.size() == 1);
  // A node's children come after it, so that from the last node back each node is measured after its children, and from
  // the first on each is placed before them. The root is the screen, whose size is given.
  for (std::size_t node = m_nodes.size(); node-- > 0;)
    MeasureWidth(m_nodes[node], _metrics);
  m_nodes[ROOT].widthUnits = m_nodes[ROOT].naturalWidthUnits;
  for (const LayoutNode& node : m_nodes)
    GiveWidths(node);
  m_tallest.clear();
  for (std::size_t node = m_nodes.size(); node-- > 0;)
    MeasureHeight(m_nodes[node], _metrics);
  // A height group's nodes are measured again as tall as the tallest of them, and what holds them with them.
  for (const LayoutNode& node : m_nodes)
  {
    if (node.style.heightGroup == 0)
      continue;
    const auto group = std::ranges::find(m_tallest, node.style.heightGroup, &std::pair<std::uint32_t, float>::first);
    if (group == m_tallest.end())
      m_tallest.emplace_back(node.style.heightGroup, node.naturalHeightUnits);
    else
      group->second = std::max(group->second, node.naturalHeightUnits);
  }
  if (!m_tallest.empty())
  {
    for (std::size_t node = m_nodes.size(); node-- > 0;)
      MeasureHeight(m_nodes[node], _metrics);
  }
  m_nodes[ROOT].heightUnits = m_nodes[ROOT].naturalHeightUnits;
  for (LayoutNode& node : m_nodes)
    Place(node);
}

std::size_t Neuron::LayoutTree::Push(LayoutKind _kind, const LayoutStyle& _style)
{
  const std::size_t node = m_nodes.size();
  LayoutNode added{.kind = _kind, .style = _style};
  if (!m_open.empty())
  {
    LayoutNode& parent = m_nodes[m_open.back()];
    added.parent = m_open.back();
    if (parent.lastChild == LayoutNode::NO_NODE)
      parent.firstChild = node;
    else
      m_nodes[parent.lastChild].nextSibling = node;
    parent.lastChild = node;
  }
  m_nodes.push_back(std::move(added));
  return node;
}

void Neuron::LayoutTree::MeasureWidth(LayoutNode& _node, const TextMetrics& _metrics)
{
  const std::vector<std::size_t> flow = ChildrenOf(m_nodes, _node, false);
  float widest = 0.0f;
  float sum = 0.0f;
  for (const std::size_t child : flow)
  {
    widest = std::max(widest, m_nodes[child].naturalWidthUnits);
    sum += m_nodes[child].naturalWidthUnits;
  }
  const LayoutStyle& style = _node.style;
  float content = 0.0f;
  switch (_node.kind)
  {
  case LayoutKind::Text:
  {
    // A text cut short within a most is as wide as it is set, so that what follows it stands after its dots.
    const LayoutText& text = _node.text;
    const float most = style.mostWidthUnits - style.padding.left - style.padding.right;
    content = text.flow == TextFlow::Fit && style.mostWidthUnits < UNBOUNDED_UNITS
                ? _metrics.Width(text.font, _metrics.Fit(text.font, text.text, most, text.trackingUnits), text.trackingUnits)
                : _metrics.Width(text.font, text.text, text.trackingUnits);
    break;
  }
  case LayoutKind::Column:
  case LayoutKind::Stack:
    content = widest;
    break;
  case LayoutKind::Row:
    content = sum + Gaps(flow.size(), style.gapUnits);
    break;
  case LayoutKind::Grid:
    content = (widest * static_cast<float>(ColumnsOf(style))) + Gaps(ColumnsOf(style), style.gapUnits);
    break;
  case LayoutKind::Box:
    break;
  }
  _node.naturalWidthUnits =
    style.widthUnits.value_or(Held(content + style.padding.left + style.padding.right, style.leastWidthUnits, style.mostWidthUnits));
}

void Neuron::LayoutTree::GiveWidths(const LayoutNode& _node)
{
  const LayoutStyle& style = _node.style;
  const float room = std::max(0.0f, _node.widthUnits - style.padding.left - style.padding.right);
  const std::vector<std::size_t> flow = ChildrenOf(m_nodes, _node, false);
  if (_node.kind == LayoutKind::Row)
  {
    // Each child first takes what it would, as far as the row goes; then the room left, or lacked, is shared among those
    // that grow.
    std::vector<Share> shares;
    for (const std::size_t child : flow)
    {
      const LayoutStyle& cell = m_nodes[child].style;
      shares.push_back({.baseUnits = Across(m_nodes[child], room, LayoutAlign::Start),
                        .grow = cell.widthUnits.has_value() ? 0.0f : cell.grow,
                        .leastUnits = cell.leastWidthUnits,
                        .mostUnits = cell.mostWidthUnits});
    }
    ShareOut(shares, room - Gaps(flow.size(), style.gapUnits));
    for (std::size_t cell = 0; cell < flow.size(); ++cell)
      m_nodes[flow[cell]].widthUnits = shares[cell].units;
  }
  else if (_node.kind == LayoutKind::Grid)
  {
    const std::size_t columns = ColumnsOf(style);
    const float column = std::max(0.0f, (room - Gaps(columns, style.gapUnits)) / static_cast<float>(columns));
    for (const std::size_t child : flow)
      m_nodes[child].widthUnits = Across(m_nodes[child], column, style.alignX);
  }
  else
  {
    for (const std::size_t child : flow)
      m_nodes[child].widthUnits = Across(m_nodes[child], room, style.alignX);
  }
  for (const std::size_t child : ChildrenOf(m_nodes, _node, true))
  {
    const LayoutEdges& margin = m_nodes[child].style.margin;
    m_nodes[child].widthUnits = Across(m_nodes[child], std::max(0.0f, room - margin.left - margin.right), LayoutAlign::Start);
  }
}

void Neuron::LayoutTree::MeasureHeight(LayoutNode& _node, const TextMetrics& _metrics)
{
  const LayoutStyle& style = _node.style;
  const std::vector<std::size_t> flow = ChildrenOf(m_nodes, _node, false);
  float tallest = 0.0f;
  float sum = 0.0f;
  for (const std::size_t child : flow)
  {
    tallest = std::max(tallest, m_nodes[child].naturalHeightUnits);
    sum += m_nodes[child].naturalHeightUnits;
  }
  float content = 0.0f;
  switch (_node.kind)
  {
  case LayoutKind::Text:
  {
    const LayoutText& text = _node.text;
    const float room = std::max(0.0f, _node.widthUnits - style.padding.left - style.padding.right);
    std::vector<std::string> set;
    if (text.flow == TextFlow::Wrap)
      set = _metrics.Wrap(text.font, text.text, room, text.trackingUnits);
    else if (std::string line = text.flow == TextFlow::Fit ? _metrics.Fit(text.font, text.text, room, text.trackingUnits) : text.text;
             !line.empty())
      set.push_back(std::move(line));
    _node.lines.clear();
    for (std::string& line : set)
    {
      const float width = _metrics.Width(text.font, line, text.trackingUnits);
      _node.lines.push_back({.text = std::move(line), .widthUnits = width});
    }
    // A text of nothing, or of which nothing fits, still holds its line, so that what follows it keeps its place.
    content = static_cast<float>(std::max<std::size_t>(_node.lines.size(), 1)) * text.lineUnits;
    break;
  }
  case LayoutKind::Column:
    content = sum + Gaps(flow.size(), style.gapUnits);
    break;
  case LayoutKind::Row:
  case LayoutKind::Stack:
    content = tallest;
    break;
  case LayoutKind::Grid:
  {
    const std::vector<float> rows = RowHeights(m_nodes, flow, ColumnsOf(style));
    for (const float row : rows)
      content += row;
    content += Gaps(rows.size(), style.gapUnits);
    break;
  }
  case LayoutKind::Box:
    break;
  }
  _node.naturalHeightUnits =
    style.heightUnits.value_or(Held(content + style.padding.top + style.padding.bottom, style.leastHeightUnits, style.mostHeightUnits));
  if (const auto group = std::ranges::find(m_tallest, style.heightGroup, &std::pair<std::uint32_t, float>::first);
      style.heightGroup != 0 && group != m_tallest.end())
    _node.naturalHeightUnits = std::max(_node.naturalHeightUnits, group->second);
}

void Neuron::LayoutTree::Place(LayoutNode& _node)
{
  const LayoutStyle& style = _node.style;
  const float left = _node.leftUnits + style.padding.left;
  const float top = _node.topUnits + style.padding.top;
  const float roomX = std::max(0.0f, _node.widthUnits - style.padding.left - style.padding.right);
  const float roomY = std::max(0.0f, _node.heightUnits - style.padding.top - style.padding.bottom);
  const std::vector<std::size_t> flow = ChildrenOf(m_nodes, _node, false);
  switch (_node.kind)
  {
  case LayoutKind::Text:
  {
    const float block = static_cast<float>(_node.lines.size()) * _node.text.lineUnits;
    float lineTop = top + Offset(style.alignY, roomY - block);
    for (LayoutLine& line : _node.lines)
    {
      line.leftUnits = left + Offset(style.alignX, roomX - line.widthUnits);
      line.topUnits = lineTop;
      lineTop += _node.text.lineUnits;
    }
    break;
  }
  case LayoutKind::Column:
  {
    // Each child first takes its own height; then the room left, or lacked, is shared among those that grow.
    std::vector<Share> shares;
    for (const std::size_t child : flow)
    {
      const LayoutStyle& cell = m_nodes[child].style;
      shares.push_back({.baseUnits = m_nodes[child].naturalHeightUnits,
                        .grow = cell.heightUnits.has_value() ? 0.0f : cell.grow,
                        .leastUnits = cell.leastHeightUnits,
                        .mostUnits = cell.mostHeightUnits});
    }
    ShareOut(shares, roomY - Gaps(flow.size(), style.gapUnits));
    float used = Gaps(flow.size(), style.gapUnits);
    for (std::size_t cell = 0; cell < flow.size(); ++cell)
    {
      m_nodes[flow[cell]].heightUnits = shares[cell].units;
      used += shares[cell].units;
    }
    float cellTop = top + Offset(style.alignY, roomY - used);
    for (const std::size_t child : flow)
    {
      LayoutNode& cell = m_nodes[child];
      cell.leftUnits = left + Offset(style.alignX, roomX - cell.widthUnits);
      cell.topUnits = cellTop;
      cellTop += cell.heightUnits + style.gapUnits;
    }
    break;
  }
  case LayoutKind::Row:
  {
    float used = Gaps(flow.size(), style.gapUnits);
    for (const std::size_t child : flow)
      used += m_nodes[child].widthUnits;
    float cellLeft = left + Offset(style.alignX, roomX - used);
    for (const std::size_t child : flow)
    {
      LayoutNode& cell = m_nodes[child];
      cell.heightUnits = Down(cell, roomY, style.alignY);
      cell.leftUnits = cellLeft;
      cell.topUnits = top + Offset(style.alignY, roomY - cell.heightUnits);
      cellLeft += cell.widthUnits + style.gapUnits;
    }
    break;
  }
  case LayoutKind::Grid:
  {
    const std::size_t columns = ColumnsOf(style);
    const float column = std::max(0.0f, (roomX - Gaps(columns, style.gapUnits)) / static_cast<float>(columns));
    const std::vector<float> rows = RowHeights(m_nodes, flow, columns);
    float rowTop = top;
    for (std::size_t index = 0; index < flow.size(); ++index)
    {
      const std::size_t row = index / columns;
      if (index > 0 && index % columns == 0)
        rowTop += rows[row - 1] + style.gapUnits;
      LayoutNode& cell = m_nodes[flow[index]];
      const auto columnIndex = static_cast<float>(index % columns);
      cell.heightUnits = Down(cell, rows[row], style.alignY);
      cell.leftUnits = left + (columnIndex * (column + style.gapUnits)) + Offset(style.alignX, column - cell.widthUnits);
      cell.topUnits = rowTop + Offset(style.alignY, rows[row] - cell.heightUnits);
    }
    break;
  }
  case LayoutKind::Stack:
    for (const std::size_t child : flow)
    {
      LayoutNode& cell = m_nodes[child];
      cell.heightUnits = Down(cell, roomY, style.alignY);
      cell.leftUnits = left + Offset(style.alignX, roomX - cell.widthUnits);
      cell.topUnits = top + Offset(style.alignY, roomY - cell.heightUnits);
    }
    break;
  case LayoutKind::Box:
    break;
  }
  for (const std::size_t child : ChildrenOf(m_nodes, _node, true))
  {
    LayoutNode& anchored = m_nodes[child];
    const LayoutEdges& margin = anchored.style.margin;
    const LayoutAnchor anchor = anchored.style.anchor.value_or(LayoutAnchor::TopLeft);
    anchored.heightUnits = anchored.naturalHeightUnits;
    // One larger than its box stands at the box's start, so that its top-left corner, where a window's title bar is, stays
    // in it.
    const float spareX = std::max(0.0f, roomX - margin.left - margin.right - anchored.widthUnits);
    const float spareY = std::max(0.0f, roomY - margin.top - margin.bottom - anchored.heightUnits);
    anchored.leftUnits = left + margin.left + Offset(AnchorX(anchor), spareX);
    anchored.topUnits = top + margin.top + Offset(AnchorY(anchor), spareY);
  }
}
