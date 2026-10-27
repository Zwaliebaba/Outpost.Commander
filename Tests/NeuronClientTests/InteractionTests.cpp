#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
namespace
{
using Neuron::FieldKey;
using Neuron::FieldStroke;
using Neuron::Interaction;
using Neuron::NO_WIDGET;
using Neuron::WidgetId;

constexpr WidgetId BUTTON = 11;
constexpr WidgetId BEHIND = 12;
constexpr WidgetId FRONT = 13;
constexpr WidgetId LIST = 14;
constexpr WidgetId FIELD = 15;
constexpr WidgetId ITEM = 16;

// A HUD panel from (0, 0) to (200, 100), a button over its left half and another over its right; and a window from
// (150, 50) to (350, 250), over the panel's right part, with a button over its top half and, under it, a list that
// scrolls and holds a text field.
Neuron::DrawList Screen()
{
  Neuron::DrawList list;
  list.panels.push_back({.left = 0.0f, .top = 0.0f, .width = 200.0f, .height = 100.0f});
  list.regions.push_back({.area = {.left = 0.0f, .top = 0.0f, .width = 100.0f, .height = 100.0f}, .widget = BUTTON});
  list.regions.push_back({.area = {.left = 100.0f, .top = 0.0f, .width = 100.0f, .height = 100.0f}, .widget = BEHIND});
  list.BeginLayer({.left = 150.0f, .top = 50.0f, .width = 200.0f, .height = 200.0f});
  list.panels.push_back({.left = 150.0f, .top = 50.0f, .width = 200.0f, .height = 200.0f});
  list.regions.push_back({.area = {.left = 150.0f, .top = 50.0f, .width = 200.0f, .height = 100.0f}, .widget = FRONT});
  list.regions.push_back(
    {.area = {.left = 150.0f, .top = 150.0f, .width = 200.0f, .height = 100.0f}, .widget = LIST, .hoverOnly = true, .scrolls = true});
  list.regions.push_back({.area = {.left = 160.0f, .top = 160.0f, .width = 50.0f, .height = 20.0f}, .widget = FIELD});
  return list;
}
} // namespace

TEST_CLASS(InteractionTests)
{
public:
  // ADR-100: the widget under the pointer is hot, and the one pressed is active until the release, holding the pointer:
  // moved off, it is not hot, nor is any other; moved back, it is again; let go over it, it is clicked, and let go
  // elsewhere, nothing is.
  TEST_METHOD(KeepsHotAndActiveAcrossFrames)
  {
    const Neuron::DrawList list = Screen();
    Interaction ui;
    ui.Move(list, 50.0f, 50.0f);
    Assert::AreEqual(BUTTON, ui.Hot());
    Assert::IsTrue(ui.Press(list, 50.0f, 50.0f), L"a press on a panel is the canvas's");
    Assert::AreEqual(BUTTON, ui.Active());

    ui.Move(list, 500.0f, 500.0f);
    Assert::AreEqual(NO_WIDGET, ui.Hot(), L"off it");
    Assert::AreEqual(BUTTON, ui.Active(), L"and still held");
    ui.Move(list, 120.0f, 50.0f);
    Assert::AreEqual(NO_WIDGET, ui.Hot(), L"no other widget is hot while one is held");
    ui.Move(list, 60.0f, 60.0f);
    Assert::AreEqual(BUTTON, ui.Hot(), L"back on it");
    Assert::AreEqual(BUTTON, ui.Release(list, 60.0f, 60.0f), L"let go over it");
    Assert::AreEqual(NO_WIDGET, ui.Active());
    Assert::AreEqual(BUTTON, ui.Hot());

    Assert::IsTrue(ui.Press(list, 50.0f, 50.0f));
    ui.Move(list, 120.0f, 50.0f);
    Assert::AreEqual(NO_WIDGET, ui.Release(list, 120.0f, 50.0f), L"let go over another");
    Assert::AreEqual(BEHIND, ui.Hot(), L"which is hot once nothing is held");

    Assert::IsFalse(ui.Press(list, 500.0f, 500.0f), L"a press off every panel is the world's");
    Assert::AreEqual(NO_WIDGET, ui.Active());
  }

  // ADR-031 decision 4; ADR-100: a press belongs to the front layer under it, so a widget of the layer behind takes none
  // and is not hot.
  TEST_METHOD(GivesThePressToTheFrontLayer)
  {
    const Neuron::DrawList list = Screen();
    Interaction ui;
    ui.Move(list, 170.0f, 80.0f);
    Assert::AreEqual(FRONT, ui.Hot(), L"the window's, over the HUD's");
    Assert::IsTrue(ui.Press(list, 170.0f, 80.0f));
    Assert::AreEqual(FRONT, ui.Active());
    Assert::AreEqual(FRONT, ui.Release(list, 170.0f, 80.0f));
    ui.Move(list, 120.0f, 80.0f);
    Assert::AreEqual(BEHIND, ui.Hot(), L"beside the window, the HUD's");
  }

  // ADR-100: the wheel is the canvas's only over a region that scrolls, whose offset takes it; over any other panel, or
  // over the world, it is the camera's.
  TEST_METHOD(TakesTheWheelOnlyOverAScroll)
  {
    const Neuron::DrawList list = Screen();
    Interaction ui;
    Assert::IsTrue(ui.Wheel(list, 300.0f, 200.0f, 40.0f));
    Assert::IsTrue(ui.Wheel(list, 300.0f, 200.0f, -15.0f));
    Assert::AreEqual(25.0f, ui.ScrollOf(LIST));
    Assert::IsTrue(ui.Wheel(list, 170.0f, 165.0f, 10.0f), L"over a field in the list");
    Assert::AreEqual(35.0f, ui.ScrollOf(LIST));
    Assert::IsFalse(ui.Wheel(list, 50.0f, 50.0f, 40.0f), L"over a panel that does not scroll");
    Assert::IsFalse(ui.Wheel(list, 500.0f, 500.0f, 40.0f), L"over the world");
    Assert::AreEqual(35.0f, ui.ScrollOf(LIST));
    Assert::AreEqual(0.0f, ui.ScrollOf(BUTTON));
    ui.SetScroll(LIST, 0.0f);
    Assert::AreEqual(0.0f, ui.ScrollOf(LIST), L"the scroll view holds its offset to its content");
  }

  // ADR-017 decision 8; ADR-100: a field takes the keyboard, and then every character and its keys; Enter and Esc give
  // it back, and so does a press anywhere but the field. Esc reaches the field first, then an open drop-down, and only
  // then goes on to the windows and the controls.
  TEST_METHOD(GivesTheKeyboardToAFieldAndEscFirstToIt)
  {
    const Neuron::DrawList list = Screen();
    Interaction ui;
    Assert::IsFalse(ui.Type(U'a'), L"no field has the keyboard");
    Assert::IsTrue(ui.Press(list, 170.0f, 165.0f));
    ui.Focus(FIELD);
    Assert::IsTrue(ui.TakesKeys());
    Assert::IsTrue(ui.Type(U'a'));
    Assert::IsTrue(ui.Type(U'b'));
    Assert::IsTrue(ui.Key(FieldKey::Backspace));
    Assert::IsTrue(ui.Type(U'c'));
    Assert::IsTrue(ui.TakeStrokes(FIELD) ==
                     std::vector<FieldStroke>{{.character = U'a'}, {.character = U'b'}, {.key = FieldKey::Backspace}, {.character = U'c'}},
                   L"in the order they came");
    Assert::IsTrue(ui.TakeStrokes(FIELD).empty(), L"taken once");
    Assert::IsTrue(ui.Type(U'd'));
    Assert::IsTrue(ui.TakeStrokes(BUTTON).empty(), L"nothing for another widget");
    Assert::IsTrue(ui.TakeStrokes(FIELD) == std::vector<FieldStroke>{{.character = U'd'}});

    ui.OpenDropDown(ITEM);
    Assert::IsTrue(ui.Escape(), L"the field first");
    Assert::IsFalse(ui.TakesKeys());
    Assert::IsTrue(ui.TakeStrokes(FIELD) == std::vector<FieldStroke>{{.key = FieldKey::Escape}}, L"which drops what was typed");
    Assert::AreEqual(ITEM, ui.DropDown(), L"the drop-down still open");
    Assert::IsTrue(ui.Escape(), L"then the drop-down");
    Assert::AreEqual(NO_WIDGET, ui.DropDown());
    Assert::IsFalse(ui.Escape(), L"then on to the windows and the controls");

    ui.Focus(FIELD);
    Assert::IsTrue(ui.Press(list, 170.0f, 165.0f), L"a press on the field");
    Assert::IsTrue(ui.TakesKeys(), L"keeps the keyboard");
    (void)ui.Release(list, 170.0f, 165.0f);
    Assert::IsFalse(ui.Press(list, 500.0f, 500.0f));
    Assert::IsFalse(ui.TakesKeys(), L"a press anywhere else ends typing");

    ui.Focus(FIELD);
    Assert::IsTrue(ui.Key(FieldKey::Enter));
    Assert::IsFalse(ui.TakesKeys(), L"so does Enter");
    Assert::IsTrue(ui.TakeStrokes(FIELD) == std::vector<FieldStroke>{{.key = FieldKey::Enter}});
  }

  // ADR-100: a widget dragged while it is active, as a scroll's thumb, takes how far down the pointer has moved since the
  // press, or since it last took it.
  TEST_METHOD(MeasuresADrag)
  {
    const Neuron::DrawList list = Screen();
    Interaction ui;
    Assert::IsTrue(ui.Press(list, 170.0f, 165.0f));
    ui.Move(list, 175.0f, 185.0f);
    Assert::AreEqual(20.0f, ui.TakeDragPixels());
    Assert::AreEqual(0.0f, ui.TakeDragPixels(), L"taken once");
    ui.Move(list, 175.0f, 175.0f);
    Assert::AreEqual(-10.0f, ui.TakeDragPixels(), L"and back up");
  }

  // ADR-100: an open drop-down's list stands in an overlay over every window; a press outside it closes it and goes no
  // further, and one inside reaches its items.
  TEST_METHOD(ClosesADropDownOnAPressOutsideIt)
  {
    Neuron::DrawList list = Screen();
    list.BeginLayer({.left = 160.0f, .top = 60.0f, .width = 100.0f, .height = 60.0f}, true);
    list.panels.push_back({.left = 160.0f, .top = 60.0f, .width = 100.0f, .height = 60.0f});
    list.regions.push_back({.area = {.left = 160.0f, .top = 60.0f, .width = 100.0f, .height = 20.0f}, .widget = ITEM});
    Interaction ui;
    ui.OpenDropDown(FRONT);
    Assert::IsTrue(ui.Press(list, 165.0f, 65.0f));
    Assert::AreEqual(ITEM, ui.Active(), L"an item of the list");
    Assert::AreEqual(ITEM, ui.Release(list, 165.0f, 65.0f));
    Assert::IsTrue(ui.Press(list, 300.0f, 200.0f), L"taken over the window under it");
    Assert::AreEqual(NO_WIDGET, ui.Active(), L"and goes no further");
    Assert::AreEqual(NO_WIDGET, ui.DropDown());
    ui.OpenDropDown(FRONT);
    Assert::IsTrue(ui.Press(list, 50.0f, 50.0f), L"and over the HUD");
    Assert::AreEqual(NO_WIDGET, ui.Active());
    Assert::AreEqual(NO_WIDGET, ui.DropDown());
    Assert::IsFalse(ui.Press(list, 500.0f, 500.0f), L"closed, a press reaches the world again");
    ui.OpenDropDown(FRONT);
    Assert::IsFalse(ui.Dismiss(list, 165.0f, 65.0f), L"a press of any button in its overlay is its");
    Assert::IsTrue(ui.Dismiss(list, 500.0f, 500.0f), L"one in the world closes it and goes no further");
    Assert::AreEqual(NO_WIDGET, ui.DropDown());
    Assert::IsFalse(ui.Dismiss(list, 500.0f, 500.0f), L"with none open, nothing is dismissed");
  }

  // ADR-100: a press on a drop-down's field opens it, and a press on it again, outside its list, closes it.
  TEST_METHOD(OpensADropDownByAPressOnItsField)
  {
    Neuron::DrawList list;
    list.panels.push_back({.left = 0.0f, .top = 0.0f, .width = 100.0f, .height = 20.0f});
    list.regions.push_back({.area = {.left = 0.0f, .top = 0.0f, .width = 100.0f, .height = 20.0f}, .widget = FRONT, .dropsDown = true});
    Interaction ui;
    Assert::IsTrue(ui.Press(list, 10.0f, 10.0f));
    Assert::AreEqual(FRONT, ui.DropDown());
    (void)ui.Release(list, 10.0f, 10.0f);
    Assert::IsTrue(ui.Press(list, 10.0f, 10.0f), L"a press on it again");
    Assert::AreEqual(NO_WIDGET, ui.DropDown(), L"closes it");
    Assert::AreEqual(NO_WIDGET, ui.Active(), L"and goes no further");
  }

  // ADR-100: an identifier is its label's or its index's under the one pushed, the same in every frame, and claimed once
  // a frame: a second claim is two widgets with one identifier.
  TEST_METHOD(NamesEachWidgetOnceAFrame)
  {
    Interaction ui;
    ui.BeginBuild();
    const WidgetId window = ui.IdOf("Production");
    ui.PushId(window);
    const WidgetId build = ui.IdOf("Build");
    const WidgetId first = ui.IdOf(std::uint64_t{0});
    ui.PopId();
    ui.PushId(ui.IdOf("Research"));
    Assert::AreNotEqual(build, ui.IdOf("Build"), L"another window's Build");
    ui.PopId();
    ui.PushId(window);
    Assert::AreEqual(build, ui.IdOf("Build"), L"the same name in the same window");
    Assert::AreNotEqual(first, ui.IdOf("0"), L"an index is not its digits");
    Assert::AreNotEqual(ui.IdOf(std::uint64_t{0x3030303030303030}), ui.IdOf("00000000"), L"nor a label of its bytes");
    Assert::AreNotEqual(NO_WIDGET, first);

    Assert::IsTrue(ui.Claim(build));
    Assert::IsTrue(ui.Claim(first));
    Assert::IsFalse(ui.Claim(build), L"claimed twice in a frame");
    ui.PopId();
    ui.BeginBuild();
    Assert::IsTrue(ui.Claim(build), L"once in the next frame");
  }

  // ADR-100: a field keeps the keyboard while each build builds it, and gives it back once one does not, as when the window
  // it is in closes, so that its keys reach the game again; and a drop-down closes once a build does not build it.
  TEST_METHOD(LetsGoOfAFieldOrADropDownNoLongerBuilt)
  {
    Interaction ui;
    ui.BeginBuild();
    Assert::IsTrue(ui.Claim(FIELD));
    ui.Focus(FIELD);
    ui.BeginBuild();
    Assert::IsTrue(ui.TakesKeys(), L"built in the last frame");
    Assert::IsTrue(ui.Claim(BUTTON));
    ui.BeginBuild();
    Assert::IsFalse(ui.TakesKeys(), L"not built in the last frame");
    Assert::IsTrue(ui.Claim(BUTTON));
    ui.OpenDropDown(BUTTON);
    ui.BeginBuild();
    Assert::IsTrue(ui.WasBuilt(BUTTON) && !ui.WasBuilt(FIELD), L"what the last build built");
    Assert::AreEqual(BUTTON, ui.DropDown(), L"a drop-down built in the last frame stays open");
    ui.BeginBuild();
    Assert::AreEqual(NO_WIDGET, ui.DropDown(), L"and closes once it is not");
  }
};
} // namespace NeuronClientTests
