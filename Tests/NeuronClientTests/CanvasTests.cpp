#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
namespace
{
using Neuron::ButtonContent;
using Neuron::Canvas;
using Neuron::CanvasFill;
using Neuron::CanvasRect;
using Neuron::DrawList;
using Neuron::FieldEnd;
using Neuron::LayoutAlign;
using Neuron::LayoutAnchor;
using Neuron::LayoutKind;
using Neuron::TextFlow;

// A color for each of the theme's roles, told apart by its red.
constexpr DirectX::XMFLOAT4 Role(float _red) noexcept
{
  return {_red, 0.0f, 0.0f, 1.0f};
}

bool Same(const DirectX::XMFLOAT4& _first, const DirectX::XMFLOAT4& _second) noexcept
{
  return _first.x == _second.x && _first.y == _second.y && _first.z == _second.z && _first.w == _second.w;
}

bool Near(float _first, float _second) noexcept
{
  return std::abs(_first - _second) < 0.01f;
}

// Whether _rect stands at (_left, _top), _width by _height, in pixels, in _color.
bool RectIs(const CanvasRect& _rect, float _left, float _top, float _width, float _height, const DirectX::XMFLOAT4& _color)
{
  return Near(_rect.left, _left) && Near(_rect.top, _top) && Near(_rect.width, _width) && Near(_rect.height, _height) &&
         Same(_rect.color, _color);
}

void AssertStandsAt(const Neuron::LayoutNode& _node, float _leftUnits, float _topUnits, const wchar_t* _what)
{
  Assert::AreEqual(_leftUnits, _node.leftUnits, 0.001f, _what);
  Assert::AreEqual(_topUnits, _node.topUnits, 0.001f, _what);
}

bool TextIs(const Neuron::CanvasText& _text, std::string_view _words, float _left, float _top, const DirectX::XMFLOAT4& _color,
            std::size_t _font)
{
  return _text.text == _words && Near(_text.left, _left) && Near(_text.top, _top) && Same(_text.color, _color) && _text.font == _font;
}

// Whether the four panels from _first are the edge round (_left, _top), _width by _height, one pixel at the tests' scale
// of two.
bool OutlineIs(const DrawList& _list, std::size_t _first, float _left, float _top, float _width, float _height,
               const DirectX::XMFLOAT4& _color)
{
  return _first + 4 <= _list.panels.size() && RectIs(_list.panels[_first], _left, _top, _width, 2.0f, _color) &&
         RectIs(_list.panels[_first + 1], _left, _top + _height - 2.0f, _width, 2.0f, _color) &&
         RectIs(_list.panels[_first + 2], _left, _top, 2.0f, _height, _color) &&
         RectIs(_list.panels[_first + 3], _left + _width - 2.0f, _top, 2.0f, _height, _color);
}

Neuron::GlyphAtlas::Font EvenFont(float _advancePixels)
{
  Neuron::GlyphAtlas::Font font;
  font.glyphs.assign(Neuron::GlyphAtlas::CHARACTER_COUNT, Neuron::GlyphAtlas::Glyph{.advance = _advancePixels});
  font.lineHeight = 2.0f * _advancePixels;
  return font;
}

constexpr std::size_t TITLE = 0;
constexpr std::size_t NAME = 1;
constexpr std::size_t LABEL = 2;
constexpr std::size_t FIGURE = 3;
constexpr std::size_t CORNER = 5;
constexpr std::size_t CHECK = 6;
constexpr std::size_t GEM = 9;

Neuron::Theme TestTheme()
{
  Neuron::Theme theme;
  theme.windowColor = Role(0.01f);
  theme.titleBarColor = Role(0.02f);
  theme.titleTextColor = Role(0.21f);
  theme.closeBoxColor = Role(0.03f);
  theme.cornerColor = Role(0.04f);
  theme.cardColor = Role(0.05f);
  theme.pickedColor = Role(0.06f);
  theme.fieldColor = Role(0.07f);
  theme.disabledColor = Role(0.08f);
  theme.edgeColor = Role(0.09f);
  theme.hoverEdgeColor = Role(0.10f);
  theme.pickedEdgeColor = Role(0.11f);
  theme.textColor = Role(0.12f);
  theme.dimTextColor = Role(0.13f);
  theme.labelColor = Role(0.14f);
  theme.keyColor = Role(0.15f);
  theme.lockedTextColor = Role(0.16f);
  theme.figureColor = Role(0.17f);
  theme.warningColor = Role(0.18f);
  theme.barTrackColor = Role(0.19f);
  theme.barFillColor = Role(0.20f);
  theme.titleFont = LABEL;
  theme.closeMarkFont = TITLE;
  theme.nameFont = NAME;
  theme.labelFont = LABEL;
  theme.figureFont = FIGURE;
  theme.cornerSprite = CORNER;
  theme.checkSprite = CHECK;
  theme.titleBarUnits = 36.0f;
  theme.titleLeftUnits = 24.0f;
  theme.titleTopUnits = 11.0f;
  theme.titleTrackingUnits = 2.0f;
  theme.closeMark = "x";
  theme.closeMarkLeftUnits = 11.0f;
  theme.closeMarkTopUnits = 5.0f;
  theme.cornerUnits = 12.0f;
  theme.edgeUnits = 1.0f;
  theme.buttonInsetUnits = 12.0f;
  theme.fitGapUnits = 6.0f;
  theme.nameLineUnits = 22.0f;
  theme.figureLineUnits = 17.0f;
  theme.figureMarkUnits = 8.0f;
  theme.figureMarkTopUnits = 2.0f;
  theme.figureMarkGapUnits = 5.0f;
  theme.keyCapUnits = 20.0f;
  theme.keyCapTextTopUnits = 2.0f;
  theme.barUnits = 3.0f;
  theme.barInsetUnits = 2.0f;
  theme.smallButtonUnits = 24.0f;
  theme.smallButtonTextTopUnits = 4.0f;
  theme.arrowGapUnits = 8.0f;
  theme.chipPadUnits = 4.0f;
  theme.checkUnits = 14.0f;
  theme.checkInsetUnits = 2.0f;
  theme.checkGapUnits = 6.0f;
  theme.scrollLaneUnits = 14.0f;
  theme.scrollThumbUnits = 6.0f;
  theme.scrollLeastThumbUnits = 12.0f;
  return theme;
}

// The fonts at a scale of two: the title's 12 units a character, the name's 10, the label's 6 and the figure's 8; and the
// theme and what every widget shares, on a screen of 960 by 540 units.
struct Fixture
{
  std::array<Neuron::GlyphAtlas::Font, 4> fonts{EvenFont(24.0f), EvenFont(20.0f), EvenFont(12.0f), EvenFont(16.0f)};
  Neuron::TextMetrics metrics{fonts, 2.0f};
  Neuron::Theme theme = TestTheme();
  Neuron::Interaction ui;

  Canvas Begin()
  {
    ui.BeginBuild();
    return {theme, metrics, ui, 960.0f, 540.0f};
  }
};
} // namespace

TEST_CLASS(CanvasTests)
{
public:
  // ADR-031 decision 3; ADR-100: a window is a layer of its own, its frame at its corner and as tall as its title bar and
  // its body, its hatched title bar with its close box's mark and its title, its close box, a bracket at each corner, and a
  // region for the title bar and one for the close box, which name nothing for the caller to do.
  TEST_METHOD(DrawsAWindowInItsLook)
  {
    Fixture fixture;
    Canvas canvas = fixture.Begin();
    const std::size_t frame = canvas.BeginWindow(7, "BUILD", {.xUnits = 100.0f, .yUnits = 50.0f}, 300.0f,
                                                 {.padding = {.left = 12.0f, .top = 12.0f, .right = 12.0f, .bottom = 12.0f}});
    (void)canvas.Tree().Add({.heightUnits = 40.0f});
    canvas.EndWindow();
    DrawList list;
    canvas.Finish(list);

    const Neuron::Theme& theme = fixture.theme;
    Assert::AreEqual(size_t{2}, list.LayerCount());
    Assert::IsTrue(RectIs(list.layers[0].frame, 200.0f, 100.0f, 600.0f, 200.0f, theme.windowColor), L"36 of title bar and 64 of body");
    Assert::AreEqual(size_t{3}, list.panels.size());
    Assert::IsTrue(RectIs(list.panels[0], 200.0f, 100.0f, 600.0f, 200.0f, theme.windowColor));
    Assert::IsTrue(RectIs(list.panels[1], 200.0f, 100.0f, 528.0f, 72.0f, theme.titleBarColor) &&
                   list.panels[1].fill == CanvasFill::Hatched);
    Assert::IsTrue(RectIs(list.panels[2], 728.0f, 100.0f, 72.0f, 72.0f, theme.closeBoxColor));
    Assert::AreEqual(size_t{2}, list.texts.size());
    Assert::IsTrue(TextIs(list.texts[0], "x", 750.0f, 110.0f, theme.textColor, TITLE), L"the close mark first");
    Assert::IsTrue(TextIs(list.texts[1], "BUILD", 248.0f, 122.0f, theme.titleTextColor, LABEL), L"then the title");
    Assert::AreEqual(4.0f, list.texts[1].trackingPixels, L"tracked as the theme has it");
    Assert::AreEqual(size_t{4}, list.sprites.size());
    const std::array<std::array<float, 2>, 4> corners{{{200.0f, 100.0f}, {200.0f, 276.0f}, {776.0f, 100.0f}, {776.0f, 276.0f}}};
    for (std::size_t corner = 0; corner < corners.size(); ++corner)
    {
      const Neuron::CanvasSprite& sprite = list.sprites[corner];
      Assert::IsTrue(sprite.sprite == CORNER &&
                     RectIs(sprite.area, corners[corner][0], corners[corner][1], 24.0f, 24.0f, theme.cornerColor));
      Assert::AreEqual(corner >= 2, sprite.mirrorX);
      Assert::AreEqual(corner % 2 == 1, sprite.mirrorY);
    }
    Assert::AreEqual(size_t{2}, list.regions.size());
    Assert::AreEqual(canvas.TitleBarOf(7), list.regions[0].widget);
    Assert::AreEqual(canvas.CloseBoxOf(7), list.regions[1].widget);
    Assert::AreEqual(Neuron::NO_PAYLOAD, list.regions[1].payload);
    Assert::AreEqual(100.0f, canvas.Tree().Node(frame).leftUnits, L"the frame's node, where it stood");
  }

  // ADR-100 decisions 2 and 6: a window not yet moved stands at its anchor of the screen, less its margin, as tall as
  // what it holds.
  TEST_METHOD(AnchorsAWindowThatHasNotMoved)
  {
    Fixture fixture;
    Canvas canvas = fixture.Begin();
    const std::size_t middle =
      canvas.BeginWindow(7, std::string(), LayoutAnchor::Middle, {.left = 16.0f, .top = 16.0f, .right = 16.0f, .bottom = 16.0f}, 300.0f);
    (void)canvas.Tree().Add({.heightUnits = 64.0f});
    canvas.EndWindow();
    const std::size_t corner = canvas.BeginWindow(8, std::string(), LayoutAnchor::BottomRight, {.right = 16.0f, .bottom = 40.0f}, 200.0f);
    (void)canvas.Tree().Add({.heightUnits = 24.0f});
    canvas.EndWindow();
    DrawList list;
    canvas.Finish(list);

    AssertStandsAt(canvas.Tree().Node(middle), 330.0f, 220.0f, L"in the middle of 960 by 540, 300 by 100");
    AssertStandsAt(canvas.Tree().Node(corner), 744.0f, 440.0f, L"16 in from the right and 40 up from the foot, 200 by 60");
    Assert::IsTrue(RectIs(list.layers[1].frame, 1488.0f, 880.0f, 400.0f, 120.0f, fixture.theme.windowColor), L"and drawn there");
    Assert::AreEqual(size_t{2}, list.texts.size(), L"no title, only the two close marks");
  }

  // ADR-100: a window's body is as wide as the window, whatever its children would take, so that a child it stretches spans
  // the window less the body's padding.
  TEST_METHOD(StretchesAWindowsBodyAcrossIt)
  {
    Fixture fixture;
    Canvas canvas = fixture.Begin();
    (void)canvas.BeginWindow(7, std::string(), {.xUnits = 100.0f, .yUnits = 50.0f}, 300.0f,
                             {.padding = {.left = 12.0f, .top = 12.0f, .right = 12.0f, .bottom = 12.0f}, .alignX = LayoutAlign::Stretch});
    const std::size_t button = canvas.Tree().Add({.heightUnits = 34.0f});
    canvas.EndWindow();
    DrawList list;
    canvas.Finish(list);

    AssertStandsAt(canvas.Tree().Node(button), 112.0f, 98.0f, L"inside the padding, under the title bar");
    Assert::AreEqual(276.0f, canvas.Tree().Node(button).widthUnits, 0.001f, L"across the window less the padding");
  }

  // ADR-100: a widget a window holds on its title bar, such as the designer's arrows, takes a press before the bar, whose
  // region and the close box's come after what the window holds.
  TEST_METHOD(PressesAWidgetOnATitleBarBeforeTheBar)
  {
    Fixture fixture;
    Canvas canvas = fixture.Begin();
    (void)canvas.BeginWindow(7, std::string(), {.xUnits = 100.0f, .yUnits = 50.0f}, 300.0f);
    (void)canvas.Tree().Begin(LayoutKind::Column, {.anchor = LayoutAnchor::TopLeft, .margin = {.left = 20.0f, .top = -30.0f}});
    const Neuron::WidgetId arrow = canvas.SmallButton("next", ">", true, 4);
    canvas.Tree().End();
    canvas.EndWindow();
    DrawList list;
    canvas.Finish(list);

    Assert::AreEqual(size_t{3}, list.regions.size());
    Assert::AreEqual(canvas.TitleBarOf(7), list.regions[1].widget, L"after the arrow");
    Assert::AreEqual(canvas.CloseBoxOf(7), list.regions[2].widget);
    const Neuron::CanvasRegion* pressed = list.HitAt(2.0f * 125.0f, 2.0f * 60.0f);
    Assert::IsTrue(pressed != nullptr && pressed->widget == arrow && pressed->payload == 4u, L"the arrow, on the bar");
    const Neuron::CanvasRegion* bar = list.HitAt(2.0f * 200.0f, 2.0f * 60.0f);
    Assert::IsTrue(bar != nullptr && bar->widget == canvas.TitleBarOf(7), L"the bar beside it");
  }

  // ADR-043 decision 1; ADR-100: a panel of the HUD is a window's body and corners, anchored, as large as what it holds,
  // and in the layer under every window.
  TEST_METHOD(DrawsAPanelOfTheHud)
  {
    Fixture fixture;
    Canvas canvas = fixture.Begin();
    (void)canvas.BeginPanel({.padding = {.left = 10.0f, .top = 10.0f, .right = 10.0f, .bottom = 10.0f},
                             .anchor = LayoutAnchor::BottomRight,
                             .margin = {.right = 16.0f, .bottom = 16.0f}});
    (void)canvas.Tree().Add({.widthUnits = 100.0f, .heightUnits = 20.0f});
    canvas.EndPanel();
    DrawList list;
    canvas.Finish(list);

    Assert::AreEqual(size_t{1}, list.LayerCount());
    Assert::AreEqual(size_t{1}, list.panels.size());
    Assert::IsTrue(RectIs(list.panels[0], 1648.0f, 968.0f, 240.0f, 80.0f, fixture.theme.windowColor), L"120 by 40, 16 in");
    Assert::AreEqual(size_t{4}, list.sprites.size());
    Assert::IsTrue(RectIs(list.sprites[3].area, 1864.0f, 1024.0f, 24.0f, 24.0f, fixture.theme.cornerColor));
  }

  // ADR-061 decision 3; ADR-100: a label sets each of its lines where the layout put it.
  TEST_METHOD(SetsALabelLineByLine)
  {
    Fixture fixture;
    Canvas canvas = fixture.Begin();
    (void)canvas.Label({.text = "Mass Driver fire rate", .font = NAME, .lineUnits = 18.0f, .flow = TextFlow::Wrap},
                       fixture.theme.labelColor, {.widthUnits = 120.0f});
    DrawList list;
    canvas.Finish(list);

    Assert::AreEqual(size_t{2}, list.texts.size());
    Assert::IsTrue(TextIs(list.texts[0], "Mass Driver", 0.0f, 0.0f, fixture.theme.labelColor, NAME));
    Assert::IsTrue(TextIs(list.texts[1], "fire rate", 0.0f, 36.0f, fixture.theme.labelColor, NAME));
  }

  // ADR-043 decision 4; ADR-100: a button is a card: its face, its edge, its label cut short only where it meets its
  // figure, the mark before its figure, its cap, and a bar of the work under way along its foot; a picked one is lit, and
  // one under the pointer has its edge lit.
  TEST_METHOD(DrawsAButtonAsACard)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    const auto build = [&fixture](DrawList& _list)
    {
      Canvas canvas = fixture.Begin();
      (void)canvas.Tree().Begin(LayoutKind::Column, {.widthUnits = 300.0f, .gapUnits = 10.0f, .alignX = LayoutAlign::Stretch});
      (void)canvas.Button("queue", {.label = "Raider", .figure = "1,200", .figureMark = GEM, .note = "FLEET CAP", .payload = 3},
                          {.heightUnits = 34.0f});
      (void)canvas.Button("picked", {.label = "Escort", .payload = 4, .picked = true}, {.heightUnits = 34.0f});
      (void)canvas.Button("build", {.label = "Build", .key = "B", .payload = 5, .progress = 0.5f}, {.heightUnits = 34.0f});
      canvas.Tree().End();
      canvas.Finish(_list);
    };
    DrawList list;
    build(list);
    Assert::AreEqual(size_t{22}, list.panels.size());
    Assert::AreEqual(size_t{5}, list.texts.size());
    Assert::AreEqual(size_t{1}, list.sprites.size(), L"the figure's mark");
    if (list.texts.size() != 5 || list.panels.size() != 22 || list.sprites.size() != 1)
      return;

    Assert::IsTrue(RectIs(list.panels[0], 0.0f, 0.0f, 600.0f, 68.0f, theme.cardColor), L"the face");
    Assert::IsTrue(OutlineIs(list, 1, 0.0f, 0.0f, 600.0f, 68.0f, theme.edgeColor), L"its edge");
    Assert::IsTrue(TextIs(list.texts[0], "Raider", 24.0f, 12.0f, theme.textColor, NAME), L"the label on the name's line, in its middle");
    Assert::IsTrue(list.sprites[0].sprite == GEM && RectIs(list.sprites[0].area, 470.0f, 21.0f, 16.0f, 16.0f, theme.figureColor),
                   L"the mark, 53 units in from the inset: 8, a gap of 5, and five characters of 8");
    Assert::IsTrue(TextIs(list.texts[1], "1,200", 496.0f, 17.0f, theme.figureColor, FIGURE), L"the figure on its own line, not a reason");
    Assert::AreEqual(fixture.ui.IdOf("queue"), list.regions[0].widget);
    Assert::AreEqual(std::uint32_t{3}, list.regions[0].payload);

    Assert::IsTrue(RectIs(list.panels[5], 0.0f, 88.0f, 600.0f, 68.0f, theme.pickedColor), L"a picked one's face");
    Assert::IsTrue(OutlineIs(list, 6, 0.0f, 88.0f, 600.0f, 68.0f, theme.pickedEdgeColor), L"and its edge");

    Assert::IsTrue(RectIs(list.panels[10], 0.0f, 176.0f, 600.0f, 68.0f, theme.cardColor));
    Assert::IsTrue(RectIs(list.panels[15], 4.0f, 234.0f, 592.0f, 6.0f, theme.barTrackColor), L"the bar's track along the foot");
    Assert::IsTrue(RectIs(list.panels[16], 4.0f, 234.0f, 296.0f, 6.0f, theme.barFillColor), L"half filled");
    Assert::IsTrue(RectIs(list.panels[17], 536.0f, 190.0f, 40.0f, 40.0f, theme.fieldColor), L"the cap");
    Assert::IsTrue(OutlineIs(list, 18, 536.0f, 190.0f, 40.0f, 40.0f, theme.keyColor));
    Assert::IsTrue(TextIs(list.texts[3], "B", 550.0f, 194.0f, theme.keyColor, LABEL), L"its letter in its middle");
    Assert::IsTrue(TextIs(list.texts[4], "Build", 24.0f, 188.0f, theme.textColor, NAME));

    fixture.ui.Move(list, 100.0f, 30.0f);
    DrawList hovered;
    build(hovered);
    Assert::IsTrue(OutlineIs(hovered, 1, 0.0f, 0.0f, 600.0f, 68.0f, theme.hoverEdgeColor), L"under the pointer");
    Assert::IsTrue(OutlineIs(hovered, 18, 536.0f, 190.0f, 40.0f, 40.0f, theme.keyColor));

    fixture.ui.Move(hovered, 100.0f, 120.0f);
    DrawList picked;
    build(picked);
    Assert::IsTrue(OutlineIs(picked, 6, 0.0f, 88.0f, 600.0f, 68.0f, theme.pickedEdgeColor), L"a picked one's edge is lit already");
  }

  // ADR-043 decision 4; ADR-100: a disabled button is dim and takes no press, and its reason stands in its figure's place,
  // or where one would stand on a button without one; without a reason, its figure is dim too.
  TEST_METHOD(DimsADisabledButtonWithItsReason)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    Canvas canvas = fixture.Begin();
    (void)canvas.Tree().Begin(LayoutKind::Column, {.widthUnits = 300.0f, .gapUnits = 10.0f, .alignX = LayoutAlign::Stretch});
    (void)canvas.Button("relay", {.label = "Relay", .figure = "300", .figureMark = GEM, .note = "NODE CAP", .payload = 6, .enabled = false},
                        {.heightUnits = 34.0f});
    (void)canvas.Button("yard", {.label = "Shipyard", .figure = "900", .figureMark = GEM, .payload = 7, .enabled = false},
                        {.heightUnits = 34.0f});
    (void)canvas.Button("give", {.label = "Give the order", .note = "NO SHIPS", .payload = 8, .enabled = false}, {.heightUnits = 34.0f});
    canvas.Tree().End();
    DrawList list;
    canvas.Finish(list);

    Assert::IsTrue(RectIs(list.panels[0], 0.0f, 0.0f, 600.0f, 68.0f, theme.fieldColor), L"dim");
    Assert::IsTrue(list.regions.empty(), L"no press");
    Assert::IsTrue(TextIs(list.texts[0], "NODE CAP", 480.0f, 17.0f, theme.labelColor, LABEL), L"the reason where the figure would be");
    Assert::IsTrue(TextIs(list.texts[1], "Relay", 24.0f, 12.0f, theme.lockedTextColor, NAME));
    Assert::AreEqual(size_t{1}, list.sprites.size(), L"no mark beside the reason");
    Assert::IsTrue(Same(list.sprites[0].area.color, theme.lockedTextColor), L"the other's mark is dim");
    Assert::IsTrue(TextIs(list.texts[3], "900", 528.0f, 105.0f, theme.lockedTextColor, FIGURE), L"and its figure");
    Assert::IsTrue(TextIs(list.texts[4], "NO SHIPS", 480.0f, 193.0f, theme.labelColor, LABEL), L"a reason with no figure at the right");

    Assert::IsTrue(fixture.ui.Press(list, 100.0f, 30.0f), L"a press on it is the canvas's");
    Assert::AreEqual(Neuron::NO_WIDGET, fixture.ui.Active(), L"but presses nothing");
  }

  // ADR-100: a box the game composes is its face, a hatch over it if it has one, and its edge, under what it holds; a card is
  // a box that takes a press naming its payload, and none without one, or one that only answers the pointer with it; an icon
  // is its sprite at its node's padded corner.
  TEST_METHOD(DrawsABoxACardAndAnIcon)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    Canvas canvas = fixture.Begin();
    (void)canvas.Tree().Begin(LayoutKind::Column, {.gapUnits = 10.0f});
    (void)canvas.BeginBox({.face = theme.fieldColor, .hatch = theme.cardColor, .edge = theme.edgeColor},
                          {.widthUnits = 100.0f, .heightUnits = 30.0f, .padding = {.left = 4.0f, .top = 2.0f}});
    (void)canvas.Icon(GEM, 10.0f, theme.figureColor, {.padding = {.top = 3.0f}});
    canvas.EndBox();
    const Neuron::WidgetId card =
      canvas.BeginCard("yard", {.face = theme.cardColor, .edge = theme.edgeColor}, 9, {.widthUnits = 100.0f, .heightUnits = 40.0f});
    canvas.EndBox();
    (void)canvas.BeginCard("locked", {.face = theme.fieldColor, .edge = theme.edgeColor}, Neuron::NO_PAYLOAD,
                           {.widthUnits = 100.0f, .heightUnits = 40.0f});
    canvas.EndBox();
    const Neuron::WidgetId preview = canvas.BeginCard("preview", {.face = theme.fieldColor, .edge = theme.edgeColor}, 7,
                                                      {.widthUnits = 100.0f, .heightUnits = 40.0f}, Neuron::CardPress::Previewed);
    canvas.EndBox();
    canvas.Tree().End();
    DrawList list;
    canvas.Finish(list);

    Assert::AreEqual(size_t{21}, list.panels.size(), L"a face, a hatch and an edge's four sides; then a face and four sides thrice");
    Assert::IsTrue(RectIs(list.panels[0], 0.0f, 0.0f, 200.0f, 60.0f, theme.fieldColor), L"the box's face");
    Assert::IsTrue(RectIs(list.panels[1], 0.0f, 0.0f, 200.0f, 60.0f, theme.cardColor) && list.panels[1].fill == CanvasFill::Hatched,
                   L"its hatch over it");
    Assert::IsTrue(RectIs(list.panels[2], 0.0f, 0.0f, 200.0f, 2.0f, theme.edgeColor), L"its edge's top");
    Assert::IsTrue(RectIs(list.panels[6], 0.0f, 80.0f, 200.0f, 80.0f, theme.cardColor), L"the card's face");
    Assert::AreEqual(size_t{2}, list.regions.size(), L"the card with a payload and the one that previews, and none for the card without");
    Assert::IsTrue(RectIs(list.regions[0].area, 0.0f, 80.0f, 200.0f, 80.0f, theme.cardColor));
    Assert::AreEqual(card, list.regions[0].widget);
    Assert::AreEqual(9u, list.regions[0].payload);
    Assert::IsFalse(list.regions[0].hoverOnly);
    Assert::AreEqual(preview, list.regions[1].widget);
    Assert::AreEqual(7u, list.regions[1].payload);
    Assert::IsTrue(list.regions[1].hoverOnly);
    Assert::IsTrue(list.HitAt(100.0f, 320.0f) == nullptr, L"which takes no press");
    Assert::IsTrue(list.PointedAt(100.0f, 320.0f) == &list.regions[1], L"but answers the pointer");
    Assert::AreEqual(size_t{1}, list.sprites.size());
    Assert::AreEqual(GEM, list.sprites[0].sprite);
    Assert::IsTrue(RectIs(list.sprites[0].area, 8.0f, 10.0f, 20.0f, 20.0f, theme.figureColor), L"in the box's padding and its own");
  }

  // ADR-068 decision 1; ADR-100: a stepper's subject stands between its arrows, cut short to leave them room, the right
  // arrow just after it; while it cannot step, its arrows are dim and take no press.
  TEST_METHOD(StepsASubjectBetweenItsArrows)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    Canvas canvas = fixture.Begin();
    canvas.Stepper("yard", {.text = "SHIPYARD 05", .font = NAME, .lineUnits = 22.0f}, theme.textColor, 200.0f, 3.0f, true, 1, 2);
    DrawList list;
    canvas.Finish(list);

    Assert::IsTrue(RectIs(list.panels[0], 0.0f, 0.0f, 48.0f, 48.0f, theme.cardColor), L"the left arrow");
    Assert::IsTrue(TextIs(list.texts[0], "<", 18.0f, 8.0f, theme.textColor, LABEL));
    Assert::IsTrue(TextIs(list.texts[1], "SHIPYARD 05", 64.0f, 6.0f, theme.textColor, NAME), L"after the arrow and a gap");
    Assert::IsTrue(RectIs(list.panels[1], 300.0f, 0.0f, 48.0f, 48.0f, theme.cardColor), L"the right arrow a gap after it");
    Assert::IsTrue(TextIs(list.texts[2], ">", 318.0f, 8.0f, theme.textColor, LABEL));
    Assert::AreEqual(size_t{2}, list.regions.size());
    Assert::AreEqual(std::uint32_t{1}, list.regions[0].payload);
    Assert::AreEqual(std::uint32_t{2}, list.regions[1].payload);

    Canvas narrow = fixture.Begin();
    narrow.Stepper("yard", {.text = "SHIPYARD 05", .font = NAME, .lineUnits = 22.0f}, theme.textColor, 120.0f, 3.0f, false, 1, 2);
    DrawList cut;
    narrow.Finish(cut);
    Assert::IsTrue(TextIs(cut.texts[1], "SH...", 64.0f, 6.0f, theme.textColor, NAME), L"cut short within 56");
    Assert::IsTrue(RectIs(cut.panels[1], 180.0f, 0.0f, 48.0f, 48.0f, theme.disabledColor), L"just after the dots, and dim");
    Assert::IsTrue(Same(cut.texts[0].color, theme.dimTextColor));
    Assert::IsTrue(cut.regions.empty());
  }

  // ADR-088 decision 3; ADR-100: a row of cells shares its width, each cell with its words in its middle, the picked one
  // lit and each naming its payload.
  TEST_METHOD(LightsThePickedCell)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    Canvas canvas = fixture.Begin();
    (void)canvas.Tree().Begin(LayoutKind::Column, {.widthUnits = 300.0f, .alignX = LayoutAlign::Stretch});
    const std::array<Neuron::LayoutText, 3> words{{{.text = "25%", .font = LABEL, .lineUnits = 16.0f},
                                                   {.text = "50%", .font = LABEL, .lineUnits = 16.0f},
                                                   {.text = "75%", .font = LABEL, .lineUnits = 16.0f}}};
    const std::array<std::uint32_t, 3> payloads{7, 8, 9};
    canvas.Cells("retreat", words, payloads, 1, 26.0f, {.gapUnits = 4.0f});
    canvas.Tree().End();
    DrawList list;
    canvas.Finish(list);

    // Each cell 292 / 3 wide, 4 apart.
    constexpr float CELL = 2.0f * 292.0f / 3.0f;
    Assert::IsTrue(RectIs(list.panels[0], 0.0f, 0.0f, CELL, 52.0f, theme.cardColor));
    Assert::IsTrue(OutlineIs(list, 1, 0.0f, 0.0f, CELL, 52.0f, theme.edgeColor));
    Assert::IsTrue(RectIs(list.panels[5], CELL + 8.0f, 0.0f, CELL, 52.0f, theme.pickedColor), L"the picked one");
    Assert::IsTrue(OutlineIs(list, 6, CELL + 8.0f, 0.0f, CELL, 52.0f, theme.pickedEdgeColor));
    Assert::IsTrue(TextIs(list.texts[1], "50%", CELL + 8.0f + ((CELL - 36.0f) / 2.0f), 10.0f, theme.textColor, LABEL),
                   L"its words in its middle");
    Assert::AreEqual(size_t{3}, list.regions.size());
    Assert::AreEqual(std::uint32_t{9}, list.regions[2].payload);
  }

  // ADR-100: a checkbox is a box, its tick while it is checked, and its label after it; a press on it names its payload.
  TEST_METHOD(TicksACheckbox)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    Canvas canvas = fixture.Begin();
    (void)canvas.Tree().Begin(LayoutKind::Column);
    (void)canvas.Checkbox("repeat", {.text = "Repeat", .font = NAME, .lineUnits = 22.0f}, true, 11);
    (void)canvas.Checkbox("hold", {.text = "Hold", .font = NAME, .lineUnits = 22.0f}, false, 12);
    canvas.Tree().End();
    DrawList list;
    canvas.Finish(list);

    Assert::IsTrue(RectIs(list.panels[0], 0.0f, 8.0f, 28.0f, 28.0f, theme.fieldColor), L"in the middle of the label's line");
    Assert::IsTrue(OutlineIs(list, 1, 0.0f, 8.0f, 28.0f, 28.0f, theme.edgeColor));
    Assert::AreEqual(size_t{1}, list.sprites.size(), L"one tick");
    Assert::IsTrue(list.sprites[0].sprite == CHECK && RectIs(list.sprites[0].area, 4.0f, 12.0f, 20.0f, 20.0f, theme.textColor));
    Assert::IsTrue(TextIs(list.texts[0], "Repeat", 40.0f, 0.0f, theme.textColor, NAME), L"a gap after the box");
    Assert::AreEqual(std::uint32_t{11}, list.regions[0].payload);
    Assert::AreEqual(std::uint32_t{12}, list.regions[1].payload);
  }

  // ADR-100: a progress bar is its track and, from its left, its fill, held to the track.
  TEST_METHOD(FillsAProgressBar)
  {
    Fixture fixture;
    Canvas canvas = fixture.Begin();
    (void)canvas.Tree().Begin(LayoutKind::Column);
    (void)canvas.ProgressBar(0.25f, {.widthUnits = 120.0f, .heightUnits = 6.0f});
    (void)canvas.ProgressBar(1.5f, {.widthUnits = 120.0f, .heightUnits = 6.0f});
    canvas.Tree().End();
    DrawList list;
    canvas.Finish(list);

    Assert::IsTrue(RectIs(list.panels[0], 0.0f, 0.0f, 240.0f, 12.0f, fixture.theme.barTrackColor));
    Assert::IsTrue(RectIs(list.panels[1], 0.0f, 0.0f, 60.0f, 12.0f, fixture.theme.barFillColor));
    Assert::IsTrue(RectIs(list.panels[3], 0.0f, 12.0f, 240.0f, 12.0f, fixture.theme.barFillColor), L"no further than full");
  }

  // ADR-085 decision 1; ADR-100: a chip is its words in the window's color on a tag of the warning's, which reaches 4 units
  // beyond them and covers their line; nothing else may fill the warning's color.
  TEST_METHOD(SetsAWarningOnlyOnAChip)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    Canvas canvas = fixture.Begin();
    (void)canvas.Tree().Begin(LayoutKind::Column, {.widthUnits = 300.0f, .alignX = LayoutAlign::Stretch});
    (void)canvas.Button("start", {.label = "Start", .payload = 1}, {.heightUnits = 34.0f});
    (void)canvas.Chip({.text = "IDLE", .font = LABEL, .lineUnits = 17.0f});
    (void)canvas.Label({.text = "Research Lab", .font = NAME, .lineUnits = 22.0f}, theme.textColor);
    canvas.Tree().End();
    DrawList list;
    canvas.Finish(list);

    const auto warned =
      std::ranges::count_if(list.panels, [&theme](const CanvasRect& _panel) { return Same(_panel.color, theme.warningColor); });
    Assert::AreEqual(std::ptrdiff_t{1}, warned, L"one tag");
    Assert::IsTrue(RectIs(list.panels.back(), 0.0f, 68.0f, 64.0f, 34.0f, theme.warningColor), L"four characters of 6 and 4 on either side");
    Assert::IsTrue(TextIs(list.texts[1], "IDLE", 8.0f, 68.0f, theme.windowColor, LABEL), L"in the window's color");
    Assert::IsFalse(Canvas::MayFill(theme, theme.warningColor), L"only a chip fills the warning's color");
    Assert::IsTrue(Canvas::MayFill(theme, theme.cardColor));
  }

  // ADR-068 decision 4; ADR-100: a key cap is its face, its edge and its letter in its middle.
  TEST_METHOD(DrawsAKeyCap)
  {
    Fixture fixture;
    Canvas canvas = fixture.Begin();
    (void)canvas.KeyCap("B");
    DrawList list;
    canvas.Finish(list);

    Assert::IsTrue(RectIs(list.panels[0], 0.0f, 0.0f, 40.0f, 40.0f, fixture.theme.fieldColor));
    Assert::IsTrue(OutlineIs(list, 1, 0.0f, 0.0f, 40.0f, 40.0f, fixture.theme.keyColor));
    Assert::IsTrue(TextIs(list.texts[0], "B", 14.0f, 4.0f, fixture.theme.keyColor, LABEL));
  }

  // ADR-017 decision 8; ADR-100: a press on a field gives it the keyboard; what is typed edits its text in the order it
  // came, up to its limit and only what the atlas holds; Enter ends typing and keeps the text, and Esc ends it and drops
  // it. While it has the keyboard it shows a caret and a lit edge, and a refused text is in the warning's color.
  TEST_METHOD(TypesIntoAField)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    std::string name = "Rai";
    const auto build = [&fixture, &name](DrawList& _list, bool _valid = true)
    {
      Canvas canvas = fixture.Begin();
      const FieldEnd end = canvas.TextField("name", name, 6, _valid, {.widthUnits = 200.0f, .heightUnits = 38.0f});
      canvas.Finish(_list);
      return end;
    };
    DrawList idle;
    Assert::IsTrue(build(idle) == FieldEnd::Typing);
    Assert::IsTrue(RectIs(idle.panels[0], 0.0f, 0.0f, 400.0f, 76.0f, theme.fieldColor));
    Assert::IsTrue(OutlineIs(idle, 1, 0.0f, 0.0f, 400.0f, 76.0f, theme.edgeColor));
    Assert::IsTrue(TextIs(idle.texts[0], "Rai", 24.0f, 16.0f, theme.textColor, NAME));

    Assert::IsTrue(fixture.ui.Press(idle, 20.0f, 20.0f));
    DrawList pressed;
    (void)build(pressed);
    Assert::IsTrue(fixture.ui.TakesKeys(), L"the press gave it the keyboard");
    Assert::IsTrue(TextIs(pressed.texts[0], "Rai_", 24.0f, 16.0f, theme.textColor, NAME), L"with a caret");
    Assert::IsTrue(OutlineIs(pressed, 1, 0.0f, 0.0f, 400.0f, 76.0f, theme.pickedEdgeColor));
    (void)fixture.ui.Release(pressed, 20.0f, 20.0f);

    Assert::IsTrue(fixture.ui.Type(U'd'));
    Assert::IsTrue(fixture.ui.Type(U'\x01'));
    Assert::IsTrue(fixture.ui.Key(Neuron::FieldKey::Backspace));
    Assert::IsTrue(fixture.ui.Type(U'n'));
    Assert::IsTrue(fixture.ui.Type(U'y'));
    Assert::IsTrue(fixture.ui.Type(U'!'));
    Assert::IsTrue(fixture.ui.Type(U'?'));
    DrawList typed;
    Assert::IsTrue(build(typed) == FieldEnd::Typing);
    Assert::AreEqual(std::string("Rainy!"), name, L"in order, no control character, and no further than six");

    Assert::IsTrue(fixture.ui.Key(Neuron::FieldKey::Enter));
    DrawList entered;
    Assert::IsTrue(build(entered, false) == FieldEnd::Entered);
    Assert::IsFalse(fixture.ui.TakesKeys());
    Assert::IsTrue(TextIs(entered.texts[0], "Rainy!", 24.0f, 16.0f, theme.warningColor, NAME), L"refused, without a caret");

    fixture.ui.Focus(fixture.ui.IdOf("name"));
    Assert::IsTrue(fixture.ui.Key(Neuron::FieldKey::Backspace));
    Assert::IsTrue(fixture.ui.Escape());
    DrawList dropped;
    Assert::IsTrue(build(dropped) == FieldEnd::Dropped, L"the caller puts back what it had");
    Assert::IsFalse(fixture.ui.TakesKeys());
  }

  // ADR-100: a scroll view holds what it holds as tall as that is, and shows what its offset brings into it. What lies
  // wholly outside it is not drawn and takes no press, what lies across its edge is drawn cut to it, and a region across
  // its edge is cut to it. It takes the wheel over it, its offset held so that what it holds covers it. While there is more
  // than it shows, its track and its thumb stand in its lane, the thumb as long against the track as the view against what
  // it holds, and dragging the thumb moves the offset; with nothing more, there is neither.
  TEST_METHOD(ScrollsWhatItHolds)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    // Rows 40 units tall, each a card with a label, in a view 100 units tall and 200 wide, its lane past them.
    const auto build = [&fixture](DrawList& _list, int _rows)
    {
      Canvas canvas = fixture.Begin();
      (void)canvas.BeginScroll("list", {.widthUnits = 214.0f, .heightUnits = 100.0f});
      for (int row = 0; row < _rows; ++row)
      {
        (void)canvas.BeginCard(std::format("row {}", row), {.face = fixture.theme.cardColor, .edge = fixture.theme.edgeColor},
                               static_cast<std::uint32_t>(row), {.heightUnits = 40.0f});
        (void)canvas.Label({.text = "Row", .font = NAME, .lineUnits = 22.0f}, fixture.theme.textColor);
        canvas.EndBox();
      }
      canvas.EndScroll();
      canvas.Finish(_list);
    };
    const Neuron::WidgetId view = fixture.ui.IdOf("list");
    const Neuron::WidgetId thumb = Neuron::Interaction::IdUnder(view, "thumb");

    DrawList top;
    build(top, 5);
    Assert::AreEqual(size_t{3}, top.texts.size(), L"the rows at 0, 40 and 80 of 200, the third across its foot");
    Assert::AreEqual(size_t{16}, top.panels.size(), L"two cards whole, the third but its bottom edge, the track and the thumb");
    Assert::IsTrue(RectIs(top.panels[10], 0.0f, 160.0f, 400.0f, 80.0f, theme.cardColor), L"the third card whole, drawn cut");
    Assert::AreEqual(size_t{1}, top.clips.size());
    Assert::IsTrue(RectIs(top.clips[0].area, 0.0f, 0.0f, 400.0f, 200.0f, {}), L"cut to the view, short of its lane");
    Assert::IsTrue(top.ClipOf(&Neuron::CanvasClip::panels, 10) != nullptr && top.ClipOf(&Neuron::CanvasClip::texts, 2) != nullptr);
    Assert::IsTrue(RectIs(top.panels[14], 408.0f, 0.0f, 12.0f, 200.0f, theme.barTrackColor), L"the track in the middle of its lane");
    Assert::IsTrue(RectIs(top.panels[15], 408.0f, 0.0f, 12.0f, 100.0f, theme.barFillColor), L"the thumb, 100 of 200 long, at the top");
    Assert::AreEqual(size_t{5}, top.regions.size(), L"three rows, the view and the thumb");
    Assert::IsTrue(RectIs(top.regions[2].area, 0.0f, 160.0f, 400.0f, 40.0f, theme.cardColor), L"the third row's cut to the view");
    Assert::IsTrue(top.regions[3].widget == view && top.regions[3].scrolls && top.regions[3].hoverOnly, L"the view takes the wheel only");
    Assert::AreEqual(thumb, top.regions[4].widget);

    Assert::IsTrue(fixture.ui.Wheel(top, 100.0f, 100.0f, 60.0f), L"the wheel over the view is the canvas's");
    DrawList moved;
    build(moved, 5);
    Assert::AreEqual(size_t{2}, moved.texts.size(),
                     L"60 down: the labels of the rows at 80 and 120, the row at 40 shown only under its label");
    Assert::AreEqual(40.0f, moved.texts[0].top, 0.001f);
    Assert::IsTrue(RectIs(moved.panels.back(), 408.0f, 60.0f, 12.0f, 100.0f, theme.barFillColor), L"the thumb 60 of 100 down its travel");

    Assert::IsTrue(fixture.ui.Wheel(moved, 100.0f, 100.0f, 1000.0f));
    DrawList bottom;
    build(bottom, 5);
    Assert::AreEqual(100.0f, fixture.ui.ScrollOf(view), 0.001f, L"held where the last row ends at its foot");
    Assert::IsFalse(fixture.ui.Wheel(bottom, 600.0f, 100.0f, 1.0f), L"the wheel beside it is not the canvas's");

    Assert::IsTrue(fixture.ui.Press(bottom, 412.0f, 150.0f), L"on the thumb, at the bottom");
    Assert::AreEqual(thumb, fixture.ui.Active());
    fixture.ui.Move(bottom, 412.0f, 110.0f);
    DrawList dragged;
    build(dragged, 5);
    Assert::AreEqual(60.0f, fixture.ui.ScrollOf(view), 0.001f, L"20 units up its travel of 50 is 40 up the 100 spare");
    (void)fixture.ui.Release(dragged, 412.0f, 110.0f);

    DrawList fits;
    build(fits, 2);
    Assert::AreEqual(size_t{10}, fits.panels.size(), L"two cards, and no track nor thumb");
    Assert::AreEqual(size_t{3}, fits.regions.size(), L"two rows and the view");
  }

  // ADR-100: a drop-down shows its value and its mark, and a press opens its list in an overlay over everything else, as
  // wide as the field, a row as tall as the field for each choice, the pick lit: under the field, or over it where the
  // screen ends under it. A press on a choice names its payload and closes the list; Esc and a press outside it close it.
  // Taller than the room either way, it stands on the side with more, opens with the pick at its top as far as it goes, and
  // scrolls with the wheel, its rows cut to it and its thumb in its lane.
  TEST_METHOD(DropsDownAList)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    // A field 200 by 24 units with its top-left corner at (100, _topUnits), on a screen 540 units tall.
    const auto build = [&fixture](DrawList& _list, float _topUnits, std::vector<std::string> _choices, std::size_t _picked)
    {
      Canvas canvas = fixture.Begin();
      const std::string value = _choices[_picked];
      (void)canvas.DropDown(
        "pick", value, std::move(_choices), _picked, [](std::size_t _choice) { return static_cast<std::uint32_t>(10 + _choice); },
        {.widthUnits = 200.0f, .heightUnits = 24.0f, .anchor = LayoutAnchor::TopLeft, .margin = {.left = 100.0f, .top = _topUnits}});
      canvas.Finish(_list);
    };
    const std::vector<std::string> three{"One", "Two", "Three"};
    const Neuron::WidgetId field = fixture.ui.IdOf("pick");
    const Neuron::WidgetId list = Neuron::Interaction::IdUnder(field, "list");

    DrawList closed;
    build(closed, 100.0f, three, 1);
    Assert::AreEqual(size_t{0}, closed.layers.size(), L"no list while it is closed");
    Assert::IsTrue(RectIs(closed.panels[0], 200.0f, 200.0f, 400.0f, 48.0f, theme.fieldColor));
    Assert::IsTrue(OutlineIs(closed, 1, 200.0f, 200.0f, 400.0f, 48.0f, theme.edgeColor));
    Assert::AreEqual(size_t{2}, closed.texts.size());
    Assert::IsTrue(TextIs(closed.texts[0], "Two", 224.0f, 202.0f, theme.textColor, NAME), L"the value, in its middle down");
    Assert::IsTrue(TextIs(closed.texts[1], "v", 570.0f, 208.0f, theme.textColor, LABEL), L"the mark in a small button's room");
    Assert::IsTrue(closed.regions.size() == 1 && closed.regions[0].widget == field);

    Assert::IsTrue(fixture.ui.Press(closed, 300.0f, 210.0f));
    Assert::AreEqual(field, fixture.ui.DropDown(), L"a press on the field opens it");
    DrawList open;
    build(open, 100.0f, three, 1);
    (void)fixture.ui.Release(open, 300.0f, 210.0f);
    Assert::IsTrue(open.regions[0].dropsDown, L"the field, which a press opens");
    Assert::IsTrue(open.layers.size() == 1 && open.layers[0].overlay, L"its list in an overlay");
    Assert::IsTrue(RectIs(open.layers[0].frame, 200.0f, 248.0f, 400.0f, 144.0f, theme.fieldColor), L"under the field, a row each");
    Assert::IsTrue(OutlineIs(open, 1, 200.0f, 200.0f, 400.0f, 48.0f, theme.pickedEdgeColor), L"the field's edge lit while it is open");
    const Neuron::CanvasSpan rows = open.PanelsOf(1);
    Assert::IsTrue(RectIs(open.panels[rows.first + 1], 200.0f, 248.0f, 400.0f, 48.0f, theme.cardColor));
    Assert::IsTrue(RectIs(open.panels[rows.first + 2], 200.0f, 296.0f, 400.0f, 48.0f, theme.pickedColor), L"the pick lit");
    Assert::IsTrue(TextIs(open.texts[open.TextsOf(1).first + 2], "Three", 224.0f, 346.0f, theme.textColor, NAME));
    const Neuron::CanvasSpan regions = open.RegionsOf(1);
    Assert::AreEqual(size_t{4}, regions.end - regions.first, L"a row each, and the list, which takes the wheel");
    Assert::IsTrue(open.regions[regions.first + 3].widget == list && open.regions[regions.first + 3].scrolls);

    const Neuron::CanvasRegion* third = open.HitAt(300.0f, 354.0f);
    Assert::IsTrue(third != nullptr && third->payload == 12u, L"a choice names its payload");
    Assert::IsTrue(fixture.ui.Press(open, 300.0f, 354.0f), L"a press in the list is the canvas's");
    DrawList picked;
    build(picked, 100.0f, three, 2);
    (void)fixture.ui.Release(picked, 300.0f, 354.0f);
    Assert::AreEqual(Neuron::NO_WIDGET, fixture.ui.DropDown(), L"and closes it");
    Assert::AreEqual(size_t{0}, picked.layers.size());

    // Esc closes it, and a press outside it.
    Assert::IsTrue(fixture.ui.Press(picked, 300.0f, 210.0f));
    DrawList again;
    build(again, 100.0f, three, 2);
    (void)fixture.ui.Release(again, 300.0f, 210.0f);
    Assert::IsTrue(fixture.ui.Escape());
    DrawList escaped;
    build(escaped, 100.0f, three, 2);
    Assert::AreEqual(size_t{0}, escaped.layers.size(), L"Esc closes it");
    Assert::IsTrue(fixture.ui.Press(escaped, 300.0f, 210.0f));
    DrawList reopened;
    build(reopened, 100.0f, three, 2);
    (void)fixture.ui.Release(reopened, 300.0f, 210.0f);
    Assert::IsTrue(fixture.ui.Dismiss(reopened, 900.0f, 900.0f), L"a press outside it goes no further");
    Assert::AreEqual(Neuron::NO_WIDGET, fixture.ui.DropDown());

    // At the screen's foot, over the field.
    Assert::IsTrue(fixture.ui.Press(reopened, 300.0f, 210.0f));
    DrawList low;
    build(low, 480.0f, three, 0);
    (void)fixture.ui.Release(low, 300.0f, 210.0f);
    Assert::IsTrue(RectIs(low.layers.back().frame, 200.0f, 816.0f, 400.0f, 144.0f, theme.fieldColor), L"over the field");

    // Thirty choices run past the room either way: under the field, where there is more, the twenty-first at the top as far
    // as the list goes, and scrolled by the wheel.
    std::vector<std::string> thirty;
    thirty.reserve(30);
    for (int choice = 0; choice < 30; ++choice)
      thirty.push_back(std::format("C{}", choice));
    Assert::IsTrue(fixture.ui.Escape());
    DrawList shut;
    build(shut, 100.0f, thirty, 20);
    Assert::IsTrue(fixture.ui.Press(shut, 300.0f, 210.0f));
    DrawList longList;
    build(longList, 100.0f, thirty, 20);
    (void)fixture.ui.Release(longList, 300.0f, 210.0f);
    Assert::IsTrue(RectIs(longList.layers.back().frame, 200.0f, 248.0f, 400.0f, 832.0f, theme.fieldColor), L"416 units, to the foot");
    Assert::AreEqual(304.0f, fixture.ui.ScrollOf(list), 0.001f, L"720 units of rows, 304 spare");
    Assert::AreEqual(size_t{1}, longList.clips.size());
    Assert::IsTrue(RectIs(longList.clips[0].area, 200.0f, 248.0f, 372.0f, 832.0f, {}), L"its rows cut to it, short of its lane");
    Assert::AreEqual(size_t{20}, longList.RegionsOf(1).end - longList.RegionsOf(1).first, L"rows 12 to 29, the list and the thumb");
    Assert::IsTrue(RectIs(longList.panels[longList.PanelsOf(1).end - 6], 580.0f, 248.0f, 12.0f, 832.0f, theme.barTrackColor),
                   L"the track in its lane");
    Assert::IsTrue(fixture.ui.Wheel(longList, 300.0f, 400.0f, -100.0f), L"the wheel over the list is its");
    DrawList scrolled;
    build(scrolled, 100.0f, thirty, 20);
    Assert::AreEqual(204.0f, fixture.ui.ScrollOf(list), 0.001f);
    const auto rowTexts = scrolled.texts.begin() + static_cast<std::ptrdiff_t>(scrolled.TextsOf(1).first);
    const auto twenty = std::ranges::find(rowTexts, scrolled.texts.end(), std::string("C20"), &Neuron::CanvasText::text);
    Assert::IsTrue(twenty != scrolled.texts.end() && Near(twenty->top, 802.0f), L"the twenty-first row 400 units down");
  }

  // ADR-100: a field's text stands in its content box, the theme's inset in from its sides and in its middle down, so that
  // its padding keeps room for what the caller sets over the field, such as a label before the text and a count after it;
  // and it is cut short within that room.
  TEST_METHOD(KeepsAFieldsTextInItsContentBox)
  {
    Fixture fixture;
    const Neuron::Theme& theme = fixture.theme;
    Canvas canvas = fixture.Begin();
    std::string name = "Rainy days";
    (void)canvas.TextField("name", name, 20, true,
                           {.widthUnits = 200.0f, .heightUnits = 38.0f, .padding = {.left = 50.0f, .top = 4.0f, .right = 60.0f}});
    DrawList list;
    canvas.Finish(list);

    Assert::IsTrue(RectIs(list.panels[0], 0.0f, 0.0f, 400.0f, 76.0f, theme.fieldColor), L"the field across its padding");
    // 66 units of room between the padding, less the inset either side: three letters of 10 and the dots.
    Assert::IsTrue(TextIs(list.texts[0], "Rai...", 124.0f, 20.0f, theme.textColor, NAME));
  }
};
} // namespace NeuronClientTests
