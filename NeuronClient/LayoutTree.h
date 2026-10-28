#pragma once

namespace Neuron
{
// What a node of the layout is (ADR-100): a box, which holds nothing of its own and which a widget draws into, or a spacer
// when it grows; a column, which sets what it holds one under another; a row, one beside another; a grid, in rows of its
// columns' count, every cell as wide as the others; a stack, each over the others; a text; or a scroll, which holds what
// it holds across it and as tall as that is, however tall it is itself, scrolled by its offset.
enum class LayoutKind : std::uint8_t
{
  Box,
  Column,
  Row,
  Grid,
  Stack,
  Text,
  Scroll
};

// How what a node holds sits across its content box on one axis: against its start, in its middle, against its end, or
// stretched across it. Along a column's or a row's own axis, stretched is the start.
enum class LayoutAlign : std::uint8_t
{
  Start,
  Center,
  End,
  Stretch
};

// Where an anchored node stands in its parent's content box: at a corner, at the middle of an edge, or in the middle.
enum class LayoutAnchor : std::uint8_t
{
  TopLeft,
  Top,
  TopRight,
  Left,
  Middle,
  Right,
  BottomLeft,
  Bottom,
  BottomRight
};

// How a text takes the width it is given: as one line, whatever its width; as one line cut short with three dots when it
// does not fit, and as wide as it is then set when it has a most; or broken between its words into lines (ADR-061
// decision 3).
enum class TextFlow : std::uint8_t
{
  Line,
  Fit,
  Wrap
};

// A distance in from each edge of a box, in reference units.
struct LayoutEdges
{
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
};

// A size with no most.
inline constexpr float UNBOUNDED_UNITS = std::numeric_limits<float>::infinity();

// How a node is sized, and how it sets out what it holds, in reference units. A size given is the size; a size not given
// is what the node holds and its padding, held between its least and its most.
struct LayoutStyle
{
  std::optional<float> widthUnits;
  std::optional<float> heightUnits;
  float leastWidthUnits = 0.0f;
  float mostWidthUnits = UNBOUNDED_UNITS;
  float leastHeightUnits = 0.0f;
  float mostHeightUnits = UNBOUNDED_UNITS;
  // Between the box's edges and what it holds, its content box.
  LayoutEdges padding;
  // Between each two of what a column, a row or a grid holds, and between a grid's rows.
  float gapUnits = 0.0f;
  // How what it holds sits across its content box: its children, or a text's lines.
  LayoutAlign alignX = LayoutAlign::Start;
  LayoutAlign alignY = LayoutAlign::Start;
  // A column's or a row's child's share of the room left along its axis, or of the room it lacks, in proportion to the
  // others' grow; none at zero. A child of a given size neither grows nor gives up room.
  float grow = 0.0f;
  // A grid's columns.
  std::size_t columns = 1;
  // An anchored node stands out of its parent's flow, at its anchor in its parent's content box less margin, and takes no
  // room in it. One larger than that box, across or down, stands at its start that way.
  std::optional<LayoutAnchor> anchor;
  LayoutEdges margin;
  // The nodes of one height group, but group 0, which is none, are as tall as the tallest of them wherever they stand, such
  // as the cards of a page and those of the pages not shown, measured and not drawn, so that the cards keep their height
  // from page to page. A node of a group holds none of the group's.
  std::uint32_t heightGroup = 0;
  // A scroll's offset: how far what it holds stands above its top, held so that what it holds still covers it.
  float scrollUnits = 0.0f;
};

// What a text node sets: the text, in UTF-8, in the atlas's font of index font, with trackingUnits more between each two
// characters, each line lineUnits under the one before.
struct LayoutText
{
  std::string text;
  std::size_t font = 0;
  float lineUnits = 0.0f;
  float trackingUnits = 0.0f;
  TextFlow flow = TextFlow::Line;
};

// A line of a text as it is set, and where its top-left corner stands.
struct LayoutLine
{
  std::string text;
  float leftUnits = 0.0f;
  float topUnits = 0.0f;
  float widthUnits = 0.0f;
};

// A node as it was built, and once arranged, where it stands, from the screen's top-left corner in reference units.
struct LayoutNode
{
  static constexpr std::size_t NO_NODE = std::numeric_limits<std::size_t>::max();

  LayoutKind kind = LayoutKind::Box;
  LayoutStyle style;
  LayoutText text;
  std::size_t parent = NO_NODE;
  std::size_t firstChild = NO_NODE;
  std::size_t lastChild = NO_NODE;
  std::size_t nextSibling = NO_NODE;

  float leftUnits = 0.0f;
  float topUnits = 0.0f;
  float widthUnits = 0.0f;
  float heightUnits = 0.0f;
  // What it would take with the room it is given: its width, before its parent gives it one, and its height, once it has.
  float naturalWidthUnits = 0.0f;
  float naturalHeightUnits = 0.0f;
  // How tall it is at its least: what it holds, where a column's children that grow give up what room they can and a
  // scroll holds nothing, which no most makes it shorter than.
  float shrunkHeightUnits = 0.0f;
  // A scroll's offset, as it was held once placed.
  float scrolledUnits = 0.0f;
  // A text's lines, as set.
  std::vector<LayoutLine> lines;
};

// A layout, recorded afresh for each frame, measured and then placed (ADR-100). Its root is the screen, a stack of the
// screen's size, and what is added goes into the container last begun and not yet ended. Arrange measures from the
// leaves up and places from the root down, once across and once down: widths go down from the screen, which is what a text
// wraps to, and heights come up from what each node holds. It needs no GPU and no window, only the text's measure.
class LayoutTree
{
public:
  static constexpr std::size_t ROOT = 0;

  LayoutTree(float _screenWidthUnits, float _screenHeightUnits);

  // Opens a column, a row, a grid or a stack under the container open, which takes what is added until End; its index.
  std::size_t Begin(LayoutKind _kind, const LayoutStyle& _style = {});
  // Closes the container last begun.
  void End();
  // A box under the container open, and its index.
  std::size_t Add(const LayoutStyle& _style = {});
  // A box that takes _grow's share of the room left, and its index.
  std::size_t AddSpacer(float _grow = 1.0f);
  // A text under the container open, and its index: as tall as its lines, and as one when nothing of it is set.
  std::size_t AddText(LayoutText _text, const LayoutStyle& _style = {});

  // Measures and places every node, each text set as _metrics sets it. Every container begun must have been ended.
  void Arrange(const TextMetrics& _metrics);

  [[nodiscard]] const LayoutNode& Node(std::size_t _node) const
  {
    return m_nodes.at(_node);
  }
  [[nodiscard]] std::span<const LayoutNode> Nodes() const noexcept
  {
    return m_nodes;
  }

private:
  std::size_t Push(LayoutKind _kind, const LayoutStyle& _style);

  void MeasureWidth(LayoutNode& _node, const TextMetrics& _metrics);
  void GiveWidths(const LayoutNode& _node);
  void MeasureHeight(LayoutNode& _node, const TextMetrics& _metrics);
  void Place(LayoutNode& _node);

  std::vector<LayoutNode> m_nodes;
  // The containers open, the innermost last; the root first.
  std::vector<std::size_t> m_open;
  // Each height group's tallest node, once measured, and how tall it is.
  std::vector<std::pair<std::uint32_t, float>> m_tallest;
};
} // namespace Neuron
