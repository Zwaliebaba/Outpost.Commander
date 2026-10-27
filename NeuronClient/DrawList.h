#pragma once

namespace Neuron
{
// How a rectangle of the canvas is filled (ADR-100): solid; with diagonal stripes, as UiPipeline::FillHatched draws them
// (ADR-030 decision 6); or with the image UiPipeline::SetImage set, taken as coverage (ADR-052).
enum class CanvasFill : std::uint8_t
{
  Solid,
  Hatched,
  Image
};

// A rectangle of the canvas, in back-buffer pixels from the top-left corner.
struct CanvasRect
{
  float left = 0.0f;
  float top = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  DirectX::XMFLOAT4 color{};
  CanvasFill fill = CanvasFill::Solid;

  [[nodiscard]] bool Contains(float _xPixels, float _yPixels) const noexcept
  {
    return _xPixels >= left && _xPixels < left + width && _yPixels >= top && _yPixels < top + height;
  }
};

// A line of text, in UTF-8, in the atlas's font of index font, with trackingPixels more between each two characters.
struct CanvasText
{
  std::string text;
  float left = 0.0f;
  float top = 0.0f;
  DirectX::XMFLOAT4 color{};
  std::size_t font = 0;
  float trackingPixels = 0.0f;
};

// The atlas's sprite of index sprite drawn into a rectangle, in the rectangle's color, mirrored when asked (ADR-030).
struct CanvasSprite
{
  std::size_t sprite = 0;
  CanvasRect area;
  bool mirrorX = false;
  bool mirrorY = false;
};

// A solid line at any angle (ADR-015 decision 2).
struct CanvasLine
{
  ScreenSegment segment;
  DirectX::XMFLOAT4 color{};
};

// A layer's share of one of the lists, from first up to end.
struct CanvasSpan
{
  std::size_t first = 0;
  std::size_t end = 0;
};

// A widget's identifier: a hash of its window's identifier and its own label or index, as the build descends (ADR-100).
// No widget is zero.
using WidgetId = std::uint64_t;
inline constexpr WidgetId NO_WIDGET = 0;
// A region that names nothing for the caller to do, as a window's title bar.
inline constexpr std::uint32_t NO_PAYLOAD = std::numeric_limits<std::uint32_t>::max();

// Where a press or the pointer finds something: the widget it is, if any; the caller's payload, which says what a press
// does in the caller's own terms, since the canvas knows no game concept (R9); whether it answers the pointer only, taking
// no press; and whether it scrolls, taking the wheel.
struct CanvasRegion
{
  CanvasRect area;
  WidgetId widget = NO_WIDGET;
  std::uint32_t payload = NO_PAYLOAD;
  bool hoverOnly = false;
  bool scrolls = false;
};

// A layer above the first: the rectangle a press on it falls in, whether it is the overlay of an open drop-down, and where
// its share of each list starts. Its share runs to where the next layer's starts, or to the list's end.
struct CanvasLayer
{
  CanvasRect frame;
  bool overlay = false;
  std::size_t firstPanel = 0;
  std::size_t firstLine = 0;
  std::size_t firstText = 0;
  std::size_t firstSprite = 0;
  std::size_t firstRegion = 0;
};

// What the canvas draws, and where a press finds something, in layers (ADR-031 decision 4; ADR-100): the first layer,
// then each layer begun on it, back to front; each layer's panels, then its lines, then its sprites, then its texts, so
// that a layer covers the text of the layer behind it. A press belongs to the front layer under it. It holds no GPU
// state, so its tests need no device; UiPipeline::Draw draws it.
struct DrawList
{
  std::vector<CanvasRect> panels;
  std::vector<CanvasLine> lines;
  std::vector<CanvasText> texts;
  std::vector<CanvasSprite> sprites;
  std::vector<CanvasRegion> regions;
  // The layers above the first, back to front.
  std::vector<CanvasLayer> layers;

  // Starts a layer over the ones before it, whose press falls in _frame: what is added from now on is its own. An overlay
  // is an open drop-down's, which stands over every window.
  void BeginLayer(const CanvasRect& _frame, bool _overlay = false);

  // The first layer and every layer begun on it.
  [[nodiscard]] std::size_t LayerCount() const noexcept
  {
    return layers.size() + 1;
  }
  // The front layer under a point: the front one begun whose frame holds it, or the first.
  [[nodiscard]] std::size_t LayerAt(float _xPixels, float _yPixels) const noexcept;
  [[nodiscard]] CanvasSpan PanelsOf(std::size_t _layer) const noexcept;
  [[nodiscard]] CanvasSpan LinesOf(std::size_t _layer) const noexcept;
  [[nodiscard]] CanvasSpan TextsOf(std::size_t _layer) const noexcept;
  [[nodiscard]] CanvasSpan SpritesOf(std::size_t _layer) const noexcept;
  [[nodiscard]] CanvasSpan RegionsOf(std::size_t _layer) const noexcept;

  // Whether a point is on a panel of any layer, where a press belongs to the canvas and not to what lies under it
  // (ADR-015 decision 5).
  [[nodiscard]] bool Covers(float _xPixels, float _yPixels) const noexcept;
  // The region of the front layer under a point that takes a press, if any, the first added where two overlap: a region
  // of a layer behind takes none.
  [[nodiscard]] const CanvasRegion* HitAt(float _xPixels, float _yPixels) const noexcept;
  // What the pointer is over in the front layer: the region HitAt finds, or else the first that answers the pointer only.
  [[nodiscard]] const CanvasRegion* PointedAt(float _xPixels, float _yPixels) const noexcept;
  // The region of the front layer under a point that takes the wheel, if any: the last added, the innermost of those that
  // hold one another.
  [[nodiscard]] const CanvasRegion* ScrollAt(float _xPixels, float _yPixels) const noexcept;
};

// The part of the line from _from to _to that lies inside _rect, by Liang and Barsky's clipping; none when none of it does.
[[nodiscard]] std::optional<std::pair<DirectX::XMFLOAT2, DirectX::XMFLOAT2>> ClipToRect(DirectX::XMFLOAT2 _from, DirectX::XMFLOAT2 _to,
                                                                                        const CanvasRect& _rect) noexcept;
} // namespace Neuron
