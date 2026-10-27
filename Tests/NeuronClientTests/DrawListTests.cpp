#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
namespace
{
using Neuron::CanvasRect;

// A list of two layers over the first: the first holds a panel and a region at the screen's top-left; the second a frame
// at (100, 100) to (300, 300) with a panel, a line, a sprite, a text, a region that takes a press and one that answers the
// pointer only; the third a frame at (200, 200) to (400, 400) with a panel and a region.
Neuron::DrawList TwoLayers()
{
  Neuron::DrawList list;
  list.panels.push_back({.left = 0.0f, .top = 0.0f, .width = 50.0f, .height = 50.0f});
  list.regions.push_back({.area = {.left = 0.0f, .top = 0.0f, .width = 50.0f, .height = 50.0f}, .payload = 1});

  list.BeginLayer({.left = 100.0f, .top = 100.0f, .width = 200.0f, .height = 200.0f});
  list.panels.push_back({.left = 100.0f, .top = 100.0f, .width = 200.0f, .height = 200.0f});
  list.lines.push_back({.segment = {.from = {110.0f, 110.0f}, .to = {120.0f, 120.0f}}});
  list.sprites.push_back({.sprite = 2, .area = {.left = 100.0f, .top = 100.0f, .width = 12.0f, .height = 12.0f}});
  list.texts.push_back({.text = "back", .left = 110.0f, .top = 110.0f, .font = 1});
  list.regions.push_back({.area = {.left = 100.0f, .top = 100.0f, .width = 100.0f, .height = 100.0f}, .payload = 2});
  list.regions.push_back({.area = {.left = 200.0f, .top = 100.0f, .width = 100.0f, .height = 100.0f}, .payload = 3, .hoverOnly = true});

  list.BeginLayer({.left = 200.0f, .top = 200.0f, .width = 200.0f, .height = 200.0f});
  list.panels.push_back({.left = 200.0f, .top = 200.0f, .width = 200.0f, .height = 200.0f});
  list.regions.push_back({.area = {.left = 250.0f, .top = 250.0f, .width = 50.0f, .height = 50.0f}, .payload = 4});
  return list;
}

bool SpanIs(Neuron::CanvasSpan _span, std::size_t _first, std::size_t _end)
{
  return _span.first == _first && _span.end == _end;
}

// Whether _region was found, and carries _payload.
bool PayloadIs(const Neuron::CanvasRegion* _region, std::uint32_t _payload)
{
  return _region != nullptr && _region->payload == _payload;
}
} // namespace

TEST_CLASS(DrawListTests)
{
public:
  // ADR-100: each layer's share of each list runs from where it was begun to where the next was, or to the list's end.
  TEST_METHOD(SharesTheListsOutByLayer)
  {
    const Neuron::DrawList list = TwoLayers();
    Assert::AreEqual(size_t{3}, list.LayerCount());
    Assert::IsTrue(SpanIs(list.PanelsOf(0), 0, 1));
    Assert::IsTrue(SpanIs(list.PanelsOf(1), 1, 2));
    Assert::IsTrue(SpanIs(list.PanelsOf(2), 2, 3));
    Assert::IsTrue(SpanIs(list.LinesOf(0), 0, 0), L"the first layer has no line");
    Assert::IsTrue(SpanIs(list.LinesOf(1), 0, 1));
    Assert::IsTrue(SpanIs(list.LinesOf(2), 1, 1));
    Assert::IsTrue(SpanIs(list.SpritesOf(1), 0, 1));
    Assert::IsTrue(SpanIs(list.TextsOf(1), 0, 1));
    Assert::IsTrue(SpanIs(list.TextsOf(2), 1, 1));
    Assert::IsTrue(SpanIs(list.RegionsOf(0), 0, 1));
    Assert::IsTrue(SpanIs(list.RegionsOf(1), 1, 3));
    Assert::IsTrue(SpanIs(list.RegionsOf(2), 3, 4));

    const Neuron::DrawList empty;
    Assert::AreEqual(size_t{1}, empty.LayerCount());
    Assert::IsTrue(SpanIs(empty.PanelsOf(0), 0, 0));
  }

  // ADR-031 decision 4: a press belongs to the front layer under it, so a region of a layer behind takes none; a region
  // that answers the pointer only takes no press, and the pointer finds it.
  TEST_METHOD(FindsWhatThePressAndThePointerAreOn)
  {
    const Neuron::DrawList list = TwoLayers();
    Assert::AreEqual(size_t{0}, list.LayerAt(10.0f, 10.0f));
    Assert::AreEqual(size_t{1}, list.LayerAt(150.0f, 150.0f));
    Assert::AreEqual(size_t{2}, list.LayerAt(250.0f, 250.0f), L"the front one where two overlap");
    Assert::AreEqual(size_t{0}, list.LayerAt(500.0f, 500.0f));

    Assert::IsTrue(PayloadIs(list.HitAt(10.0f, 10.0f), 1));
    Assert::IsTrue(PayloadIs(list.HitAt(150.0f, 150.0f), 2));
    Assert::IsTrue(PayloadIs(list.PointedAt(150.0f, 150.0f), 2), L"the pointer finds what a press would first");
    Assert::IsNull(list.HitAt(250.0f, 150.0f), L"answers the pointer only");
    Assert::IsTrue(PayloadIs(list.PointedAt(250.0f, 150.0f), 3));
    Assert::IsTrue(PayloadIs(list.HitAt(260.0f, 260.0f), 4));
    Assert::IsNull(list.HitAt(210.0f, 210.0f), L"the layer behind's region is covered");
    Assert::IsNull(list.PointedAt(210.0f, 210.0f));

    Assert::IsTrue(list.Covers(10.0f, 10.0f));
    Assert::IsTrue(list.Covers(390.0f, 390.0f));
    Assert::IsFalse(list.Covers(60.0f, 60.0f));
  }

  // ADR-015 decision 4: a line is cut where it leaves a rectangle, kept whole inside it, and dropped outside it.
  TEST_METHOD(ClipsALineToARectangle)
  {
    const CanvasRect area{.left = 0.0f, .top = 0.0f, .width = 100.0f, .height = 100.0f};
    const auto inside = Neuron::ClipToRect({10.0f, 10.0f}, {90.0f, 50.0f}, area);
    Assert::IsTrue(inside.has_value());
    Assert::AreEqual(10.0f, inside.value_or(std::pair<DirectX::XMFLOAT2, DirectX::XMFLOAT2>{}).first.x, 0.001f);
    Assert::AreEqual(90.0f, inside.value_or(std::pair<DirectX::XMFLOAT2, DirectX::XMFLOAT2>{}).second.x, 0.001f);

    const auto across = Neuron::ClipToRect({-50.0f, 50.0f}, {150.0f, 50.0f}, area);
    Assert::IsTrue(across.has_value());
    const auto [from, to] = across.value_or(std::pair<DirectX::XMFLOAT2, DirectX::XMFLOAT2>{});
    Assert::AreEqual(0.0f, from.x, 0.001f, L"cut at the left edge");
    Assert::AreEqual(100.0f, to.x, 0.001f, L"and at the right");
    Assert::AreEqual(50.0f, from.y, 0.001f);

    Assert::IsFalse(Neuron::ClipToRect({-50.0f, -10.0f}, {150.0f, -10.0f}, area).has_value(), L"above it");
    Assert::IsFalse(Neuron::ClipToRect({150.0f, 10.0f}, {150.0f, 90.0f}, area).has_value(), L"beside it, upright");
  }
};
} // namespace NeuronClientTests
