#pragma once

namespace Outpost
{
// The first HUD (task 3.6): the Ore stockpile, and a panel describing the selection. It is laid out once in 1920×1080
// reference units, each element anchored to a corner or an edge, and scaled to the back buffer by one uniform factor
// (ADR-006). It keeps no GPU state: it says what to draw, in pixels, and GameClient draws it through the UI pipeline
// (ADR-015). Clicks on it do not reach the world.
class Hud
{
public:
  static constexpr float REFERENCE_WIDTH_UNITS = 1920.0f;
  static constexpr float REFERENCE_HEIGHT_UNITS = 1080.0f;
  // The text's size at the reference scale.
  static constexpr float FONT_UNITS = 20.0f;

  // What the HUD shows, in words.
  struct Content
  {
    std::int32_t ore = 0;
    // The selection panel's lines, first to last; none when nothing is selected.
    std::vector<std::string> selection;
  };

  struct Rect
  {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    DirectX::XMFLOAT4 color{};
  };

  struct Text
  {
    std::string text;
    float left = 0.0f;
    float top = 0.0f;
    DirectX::XMFLOAT4 color{};
  };

  // The HUD on a back buffer of one size, in its pixels.
  struct Layout
  {
    float fontPixels = 0.0f;
    std::vector<Rect> panels;
    std::vector<Text> texts;

    // Whether a point in back-buffer pixels is on a panel, where a click belongs to the HUD.
    [[nodiscard]] bool Covers(float _xPixels, float _yPixels) const noexcept;
  };

  // The content for _player: its Ore from the newest snapshot, and a description of the ships in _selected, by design
  // name from the snapshot's designs.
  [[nodiscard]] static Content Describe(const Snapshot& _newest, std::span<const EntityView> _entities,
                                        std::span<const EntityId> _selected);

  // Where everything goes on a back buffer of this size.
  [[nodiscard]] static Layout Lay(const Content& _content, std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  // The scale from reference units to pixels: the largest at which the whole reference frame fits (ADR-006).
  [[nodiscard]] static float Scale(std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept;
};

// _value with a comma between each group of three digits, such as "12,000".
[[nodiscard]] std::string WithThousands(std::int64_t _value);
} // namespace Outpost
