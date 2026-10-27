#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
namespace
{
using Neuron::LayoutAlign;
using Neuron::LayoutAnchor;
using Neuron::LayoutKind;
using Neuron::LayoutTree;
using Neuron::TextFlow;

// A font of the atlas made by hand, every character _advancePixels wide, so that a width is a count of characters.
Neuron::GlyphAtlas::Font EvenFont(float _advancePixels)
{
  Neuron::GlyphAtlas::Font font;
  font.glyphs.assign(Neuron::GlyphAtlas::CHARACTER_COUNT, Neuron::GlyphAtlas::Glyph{.advance = _advancePixels});
  font.lineHeight = 2.0f * _advancePixels;
  return font;
}

// Every character 10 units wide, at a scale of one.
struct TenUnitFont
{
  std::array<Neuron::GlyphAtlas::Font, 1> fonts{EvenFont(10.0f)};
  Neuron::TextMetrics metrics{fonts, 1.0f};
};

void AssertStands(const Neuron::LayoutNode& _node, float _left, float _top, float _width, float _height, const wchar_t* _what)
{
  Assert::AreEqual(_left, _node.leftUnits, 0.001f, _what);
  Assert::AreEqual(_top, _node.topUnits, 0.001f, _what);
  Assert::AreEqual(_width, _node.widthUnits, 0.001f, _what);
  Assert::AreEqual(_height, _node.heightUnits, 0.001f, _what);
}

// A tree with something of every kind in it, a window's worth: an anchored column of text that wraps, is cut short and
// is set as it is, a row with a spacer, a grid and a stack.
void BuildWindow(LayoutTree& _tree)
{
  (void)_tree.Begin(LayoutKind::Column, {.widthUnits = 300.0f,
                                         .padding = {.left = 12.0f, .top = 8.0f, .right = 12.0f, .bottom = 8.0f},
                                         .gapUnits = 6.0f,
                                         .alignX = LayoutAlign::Stretch,
                                         .anchor = LayoutAnchor::BottomRight,
                                         .margin = {.right = 16.0f, .bottom = 16.0f}});
  (void)_tree.AddText({.text = "Mass Driver fire rate and its reach", .lineUnits = 18.0f, .flow = TextFlow::Wrap});
  (void)_tree.Begin(LayoutKind::Row, {.gapUnits = 4.0f, .alignY = LayoutAlign::Center});
  (void)_tree.AddText({.text = "Shipyard Alpha Seven", .lineUnits = 18.0f, .flow = TextFlow::Fit}, {.grow = 1.0f});
  (void)_tree.AddSpacer();
  (void)_tree.Add({.widthUnits = 40.0f, .heightUnits = 30.0f});
  _tree.End();
  (void)_tree.Begin(LayoutKind::Grid, {.gapUnits = 2.0f, .columns = 3});
  for (int cell = 0; cell < 5; ++cell)
    (void)_tree.AddText({.text = std::format("Cell {}", cell), .lineUnits = 16.0f});
  _tree.End();
  (void)_tree.Begin(LayoutKind::Stack, {.heightUnits = 40.0f, .alignX = LayoutAlign::Center, .alignY = LayoutAlign::Center});
  (void)_tree.AddText({.text = "Build", .lineUnits = 16.0f});
  _tree.End();
  _tree.End();
}
} // namespace

TEST_CLASS(LayoutTreeTests)
{
public:
  // ADR-100: a column sets what it holds one under another and a row one beside another, inside their padding and with
  // their gap between each two; a parent is as large as what it holds and its padding.
  TEST_METHOD(SetsColumnsAndRowsWithPaddingAndGaps)
  {
    const TenUnitFont font;
    LayoutTree tree(1000.0f, 500.0f);
    const std::size_t column =
      tree.Begin(LayoutKind::Column, {.padding = {.left = 10.0f, .top = 8.0f, .right = 10.0f, .bottom = 8.0f}, .gapUnits = 4.0f});
    const std::size_t first = tree.Add({.widthUnits = 50.0f, .heightUnits = 20.0f});
    const std::size_t second = tree.Add({.widthUnits = 30.0f, .heightUnits = 10.0f});
    const std::size_t row =
      tree.Begin(LayoutKind::Row,
                 {.padding = {.left = 5.0f, .top = 5.0f, .right = 5.0f, .bottom = 5.0f}, .gapUnits = 6.0f, .alignY = LayoutAlign::Center});
    const std::size_t tall = tree.Add({.widthUnits = 40.0f, .heightUnits = 30.0f});
    const std::size_t low = tree.Add({.widthUnits = 20.0f, .heightUnits = 10.0f});
    tree.End();
    tree.End();
    tree.Arrange(font.metrics);

    AssertStands(tree.Node(column), 0.0f, 0.0f, 96.0f, 94.0f, L"76 of the row and 20 of padding; 20, 10 and 40, two gaps and 16");
    AssertStands(tree.Node(first), 10.0f, 8.0f, 50.0f, 20.0f, L"inside the padding");
    AssertStands(tree.Node(second), 10.0f, 32.0f, 30.0f, 10.0f, L"a gap under the first");
    AssertStands(tree.Node(row), 10.0f, 46.0f, 76.0f, 40.0f, L"40, 6 and 20, and 10 of padding");
    AssertStands(tree.Node(tall), 15.0f, 51.0f, 40.0f, 30.0f, L"inside the row's padding");
    AssertStands(tree.Node(low), 61.0f, 61.0f, 20.0f, 10.0f, L"a gap beside it, and in the row's middle");
    Assert::AreEqual(column, tree.Node(row).parent);
    Assert::AreEqual(row, tree.Node(low).parent);
  }

  // ADR-100: what a node holds sits across its content box at its start, in its middle or at its end, or is stretched
  // across it; along a column, the group as one.
  TEST_METHOD(AlignsWhatANodeHolds)
  {
    const TenUnitFont font;
    LayoutTree tree(1000.0f, 500.0f);
    (void)tree.Begin(LayoutKind::Row, {.alignY = LayoutAlign::Stretch});
    (void)tree.Begin(LayoutKind::Column, {.widthUnits = 200.0f, .heightUnits = 100.0f, .alignX = LayoutAlign::Center});
    const std::size_t centered = tree.Add({.widthUnits = 50.0f, .heightUnits = 10.0f});
    tree.End();
    (void)tree.Begin(LayoutKind::Column, {.widthUnits = 200.0f, .alignX = LayoutAlign::End, .alignY = LayoutAlign::End});
    const std::size_t atEnd = tree.Add({.widthUnits = 50.0f, .heightUnits = 10.0f});
    tree.End();
    (void)tree.Begin(LayoutKind::Column, {.widthUnits = 200.0f, .alignX = LayoutAlign::Stretch, .alignY = LayoutAlign::Center});
    const std::size_t stretched = tree.Add({.heightUnits = 10.0f});
    const std::size_t held = tree.Add({.heightUnits = 10.0f, .mostWidthUnits = 120.0f});
    tree.End();
    (void)tree.Begin(LayoutKind::Stack, {.widthUnits = 100.0f, .alignX = LayoutAlign::End, .alignY = LayoutAlign::Center});
    const std::size_t stacked = tree.Add({.widthUnits = 20.0f, .heightUnits = 20.0f});
    tree.End();
    tree.End();
    tree.Arrange(font.metrics);

    AssertStands(tree.Node(centered), 75.0f, 0.0f, 50.0f, 10.0f, L"in the middle across, at the top down");
    AssertStands(tree.Node(atEnd), 350.0f, 90.0f, 50.0f, 10.0f, L"stretched to the row's 100, at the end of both");
    AssertStands(tree.Node(stretched), 400.0f, 40.0f, 200.0f, 10.0f, L"across the column, the pair in its middle");
    AssertStands(tree.Node(held), 400.0f, 50.0f, 120.0f, 10.0f, L"stretched no further than its most");
    AssertStands(tree.Node(stacked), 680.0f, 40.0f, 20.0f, 20.0f, L"at the stack's end across, in its middle down");
  }

  // ADR-100: the room left along a column or a row is shared among what grows, in proportion, each held between its least
  // and its most, and what one held could not take goes to the rest; the room lacked is given up the same way.
  TEST_METHOD(GrowsAndHoldsBetweenLeastAndMost)
  {
    const TenUnitFont font;
    LayoutTree tree(1000.0f, 500.0f);
    (void)tree.Begin(LayoutKind::Column, {.gapUnits = 10.0f});
    (void)tree.Begin(LayoutKind::Row, {.widthUnits = 300.0f, .gapUnits = 10.0f});
    const std::size_t left = tree.Add({.widthUnits = 50.0f});
    const std::size_t spacer = tree.AddSpacer();
    const std::size_t right = tree.Add({.widthUnits = 30.0f});
    tree.End();
    (void)tree.Begin(LayoutKind::Row, {.widthUnits = 300.0f});
    const std::size_t small = tree.Add({.mostWidthUnits = 40.0f, .grow = 1.0f});
    const std::size_t large = tree.Add({.grow = 3.0f});
    const std::size_t least = tree.Add({.leastWidthUnits = 60.0f});
    tree.End();
    (void)tree.Begin(LayoutKind::Column, {.heightUnits = 50.0f});
    (void)tree.Add({.heightUnits = 40.0f});
    const std::size_t squeezed = tree.Add({.leastHeightUnits = 20.0f, .grow = 1.0f});
    tree.End();
    (void)tree.Begin(LayoutKind::Row, {.widthUnits = 200.0f});
    const std::size_t cut = tree.AddText({.text = "Shipyard Alpha", .lineUnits = 18.0f, .flow = TextFlow::Fit}, {.grow = 1.0f});
    (void)tree.Add({.widthUnits = 100.0f});
    tree.End();
    tree.End();
    tree.Arrange(font.metrics);

    Assert::AreEqual(200.0f, tree.Node(spacer).widthUnits, L"300 less 50, 30 and two gaps");
    Assert::AreEqual(270.0f, tree.Node(right).leftUnits, L"pushed to the row's end");
    Assert::AreEqual(0.0f, tree.Node(left).leftUnits);
    Assert::AreEqual(40.0f, tree.Node(small).widthUnits, L"a quarter of 240 is more than its most");
    Assert::AreEqual(200.0f, tree.Node(large).widthUnits, L"three quarters, and what the small one could not take");
    Assert::AreEqual(60.0f, tree.Node(least).widthUnits, L"nothing in it, and held at its least");
    Assert::AreEqual(20.0f, tree.Node(squeezed).heightUnits, L"gives up what the 50 lacks, but no more than its least");
    Assert::AreEqual(100.0f, tree.Node(cut).widthUnits, L"gives up the 40 the row lacks");
    Assert::IsTrue(tree.Node(cut).lines.size() == 1 && tree.Node(cut).lines[0].text == "Shipyar...", L"and is cut short to it");
  }

  // ADR-100: a grid sets its cells in rows of its columns' count, each column as wide as the others and each row as tall
  // as its tallest cell; a stack is as large as the largest it holds.
  TEST_METHOD(SetsAGridAndAStack)
  {
    const TenUnitFont font;
    LayoutTree tree(1000.0f, 500.0f);
    (void)tree.Begin(LayoutKind::Row);
    const std::size_t grid =
      tree.Begin(LayoutKind::Grid, {.padding = {.left = 4.0f, .top = 4.0f, .right = 4.0f, .bottom = 4.0f}, .gapUnits = 2.0f, .columns = 3});
    const std::array<std::pair<float, float>, 5> sizes{{{10.0f, 10.0f}, {30.0f, 20.0f}, {20.0f, 5.0f}, {25.0f, 8.0f}, {5.0f, 12.0f}}};
    std::vector<std::size_t> cells;
    cells.reserve(sizes.size());
    for (const auto& [width, height] : sizes)
      cells.push_back(tree.Add({.widthUnits = width, .heightUnits = height}));
    tree.End();
    const std::size_t stack = tree.Begin(LayoutKind::Stack);
    (void)tree.Add({.widthUnits = 30.0f, .heightUnits = 10.0f});
    (void)tree.Add({.widthUnits = 10.0f, .heightUnits = 40.0f});
    tree.End();
    tree.End();
    tree.Arrange(font.metrics);

    AssertStands(tree.Node(grid), 0.0f, 0.0f, 102.0f, 42.0f, L"three columns of 30 and two gaps; rows of 20 and 12 and a gap");
    AssertStands(tree.Node(cells[0]), 4.0f, 4.0f, 10.0f, 10.0f, L"the first cell");
    AssertStands(tree.Node(cells[1]), 36.0f, 4.0f, 30.0f, 20.0f, L"the second column");
    AssertStands(tree.Node(cells[2]), 68.0f, 4.0f, 20.0f, 5.0f, L"the third");
    AssertStands(tree.Node(cells[3]), 4.0f, 26.0f, 25.0f, 8.0f, L"the second row, under the first's 20 and a gap");
    AssertStands(tree.Node(cells[4]), 36.0f, 26.0f, 5.0f, 12.0f, L"its second column");
    AssertStands(tree.Node(stack), 102.0f, 0.0f, 30.0f, 40.0f, L"as wide as the wider and as tall as the taller");
  }

  // ADR-100: an anchored node stands at a corner, the middle of an edge or the middle of its parent's content box, less
  // its margin, and takes no room in its parent.
  TEST_METHOD(AnchorsToEachCornerEdgeAndTheMiddle)
  {
    const TenUnitFont font;
    LayoutTree tree(1000.0f, 500.0f);
    constexpr std::array<LayoutAnchor, 9> ANCHORS{LayoutAnchor::TopLeft,    LayoutAnchor::Top,    LayoutAnchor::TopRight,
                                                  LayoutAnchor::Left,       LayoutAnchor::Middle, LayoutAnchor::Right,
                                                  LayoutAnchor::BottomLeft, LayoutAnchor::Bottom, LayoutAnchor::BottomRight};
    std::vector<std::size_t> anchored;
    anchored.reserve(ANCHORS.size());
    for (const LayoutAnchor anchor : ANCHORS)
      anchored.push_back(tree.Add({.widthUnits = 100.0f,
                                   .heightUnits = 50.0f,
                                   .anchor = anchor,
                                   .margin = {.left = 10.0f, .top = 20.0f, .right = 30.0f, .bottom = 40.0f}}));
    const std::size_t parent = tree.Begin(
      LayoutKind::Column, {.padding = {.left = 10.0f, .top = 10.0f, .right = 10.0f, .bottom = 10.0f}, .anchor = LayoutAnchor::Middle});
    (void)tree.Add({.widthUnits = 180.0f, .heightUnits = 80.0f});
    const std::size_t inParent = tree.Add({.widthUnits = 50.0f, .heightUnits = 10.0f, .anchor = LayoutAnchor::BottomRight});
    tree.End();
    tree.Arrange(font.metrics);

    // Across: 10 in from the left, in the middle of the 1000 less 40, or 30 in from the right; down the same with 20 and 40.
    constexpr std::array<float, 3> LEFTS{10.0f, 440.0f, 870.0f};
    constexpr std::array<float, 3> TOPS{20.0f, 215.0f, 410.0f};
    for (std::size_t anchor = 0; anchor < ANCHORS.size(); ++anchor)
      AssertStands(tree.Node(anchored[anchor]), LEFTS[anchor % 3], TOPS[anchor / 3], 100.0f, 50.0f, L"at its anchor");

    AssertStands(tree.Node(parent), 400.0f, 200.0f, 200.0f, 100.0f, L"measured without its anchored child, in the screen's middle");
    AssertStands(tree.Node(inParent), 540.0f, 280.0f, 50.0f, 10.0f, L"at its parent's bottom right, inside its padding");
  }

  // ADR-061 decision 3; ADR-100: a text wraps to the width it is given, or is cut short with three dots, or is set as it
  // is; its parent is as tall as its lines, and its lines sit across its box as it aligns them.
  TEST_METHOD(WrapsAndCutsText)
  {
    const TenUnitFont font;
    LayoutTree tree(1000.0f, 500.0f);
    const std::size_t column = tree.Begin(
      LayoutKind::Column,
      {.widthUnits = 120.0f, .padding = {.left = 5.0f, .top = 5.0f, .right = 5.0f, .bottom = 5.0f}, .alignX = LayoutAlign::Stretch});
    const std::size_t wrapped = tree.AddText({.text = "Mass Driver fire rate", .lineUnits = 18.0f, .flow = TextFlow::Wrap});
    const std::size_t fitted = tree.AddText({.text = "Shipyard Alpha", .lineUnits = 18.0f, .flow = TextFlow::Fit});
    const std::size_t asIs = tree.AddText({.text = "Calibration matrix", .lineUnits = 18.0f});
    const std::size_t centered = tree.AddText({.text = "fire", .lineUnits = 18.0f}, {.alignX = LayoutAlign::Center});
    const std::size_t tracked = tree.AddText({.text = "Mass Driver", .lineUnits = 18.0f, .trackingUnits = 1.0f, .flow = TextFlow::Wrap});
    tree.End();
    tree.Arrange(font.metrics);

    const Neuron::LayoutNode& wrap = tree.Node(wrapped);
    Assert::AreEqual(size_t{2}, wrap.lines.size());
    Assert::AreEqual(std::string("Mass Driver"), wrap.lines[0].text, L"110 fits the 110 within the padding");
    Assert::AreEqual(std::string("fire rate"), wrap.lines[1].text);
    Assert::AreEqual(23.0f, wrap.lines[1].topUnits, L"a line under the first");
    AssertStands(wrap, 5.0f, 5.0f, 110.0f, 36.0f, L"as tall as its two lines");

    Assert::AreEqual(std::string("Shipyard..."), tree.Node(fitted).lines.at(0).text, L"no space before the dots");
    Assert::AreEqual(180.0f, tree.Node(asIs).lines.at(0).widthUnits, L"set as it is, past its box");
    Assert::AreEqual(40.0f, tree.Node(centered).lines.at(0).leftUnits, L"40 in the middle of 110, past 5");
    Assert::AreEqual(size_t{2}, tree.Node(tracked).lines.size(), L"ten gaps of tracking take Mass Driver past 110");
    Assert::AreEqual(5.0f + 36.0f + 18.0f + 18.0f + 18.0f + 36.0f + 5.0f, tree.Node(column).heightUnits, L"its lines and padding");
  }

  // ADR-070; ADR-100: a tree is laid out in reference units, so that at twice the scale, with fonts rasterized at twice
  // their size, every node stands where it stood and only its pixels double.
  TEST_METHOD(ArrangesTheSameAtTwoScales)
  {
    const std::array<Neuron::GlyphAtlas::Font, 1> atOne{EvenFont(10.0f)};
    const std::array<Neuron::GlyphAtlas::Font, 1> atTwo{EvenFont(20.0f)};
    LayoutTree small(1920.0f, 1080.0f);
    LayoutTree large(1920.0f, 1080.0f);
    BuildWindow(small);
    BuildWindow(large);
    small.Arrange(Neuron::TextMetrics(atOne, 1.0f));
    large.Arrange(Neuron::TextMetrics(atTwo, 2.0f));

    Assert::AreEqual(small.Nodes().size(), large.Nodes().size());
    for (std::size_t node = 0; node < small.Nodes().size(); ++node)
    {
      const Neuron::LayoutNode& one = small.Node(node);
      AssertStands(large.Node(node), one.leftUnits, one.topUnits, one.widthUnits, one.heightUnits, L"the same in units");
      Assert::AreEqual(one.lines.size(), large.Node(node).lines.size());
      for (std::size_t line = 0; line < one.lines.size(); ++line)
      {
        Assert::AreEqual(one.lines[line].text, large.Node(node).lines[line].text);
        Assert::AreEqual(one.lines[line].leftUnits, large.Node(node).lines[line].leftUnits, 0.001f);
      }
    }
    // The window stands 16 in from the screen's bottom right, as wide as it was given.
    const Neuron::LayoutNode& window = small.Node(1);
    Assert::AreEqual(1920.0f - 16.0f - 300.0f, window.leftUnits, 0.001f);
    Assert::AreEqual(1080.0f - 16.0f, window.topUnits + window.heightUnits, 0.001f);
  }
};
} // namespace NeuronClientTests
