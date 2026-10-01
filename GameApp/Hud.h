#pragma once

namespace Outpost
{
// The HUD (tasks 3.6 and 4.5, design §9): the Ore stockpile and income, a panel describing the selection with a
// structure's construction and queue, the buttons that build structures and queue ships, and the minimap. It is laid
// out once in 1920×1080 reference units, each element anchored to a corner or an edge, and scaled to the back buffer by
// one uniform factor (ADR-006). It keeps no GPU state: it says what to draw, in pixels, and GameClient draws it through
// the UI pipeline (ADR-015). Clicks on it do not reach the world. The research queue arrives with research (task 5.1).
class Hud
{
public:
  static constexpr float REFERENCE_WIDTH_UNITS = 1920.0f;
  static constexpr float REFERENCE_HEIGHT_UNITS = 1080.0f;
  // The text's size at the reference scale.
  static constexpr float FONT_UNITS = 20.0f;

  // What a button does when it is pressed.
  enum class ActionKind : std::uint8_t
  {
    // Arms placing a structure for the selected Constructors.
    Build,
    // Queues a job at a Shipyard or the Command Station.
    Queue
  };

  struct Action
  {
    ActionKind kind = ActionKind::Build;
    StructureKind structure = StructureKind::CommandStation;
    EntityId producer;
    // The design a Shipyard builds; no design for the Command Station's Constructor.
    DesignId design;

    friend bool operator==(const Action&, const Action&) = default;
  };

  struct Button
  {
    std::string label;
    Action action;
    // A button the player cannot afford, or may not use, is drawn dim and does nothing.
    bool enabled = true;
  };

  // Whose a minimap mark is, which sets its color.
  enum class Side : std::uint8_t
  {
    Own,
    Enemy,
    Neutral
  };

  // One entity on the minimap: a ship as a dot, a structure as a larger square, an asteroid or field at its size.
  struct Mark
  {
    PlanePosition position;
    float radiusMeters = 0.0f;
    Side side = Side::Neutral;
    EntityKind kind = EntityKind::Ship;
  };

  // What the HUD shows, in words and marks.
  struct Content
  {
    std::int32_t ore = 0;
    std::int32_t oreIncomeHundredthsPerSecond = 0;
    // The selection panel's lines, first to last; none when nothing is selected.
    std::vector<std::string> selection;
    std::vector<Button> buttons;
    // A line at the top while a structure's placement is armed.
    std::string hint;
    // No minimap when the map's size is not known.
    float mapSizeMeters = 0.0f;
    std::vector<Mark> marks;
  };

  struct Rect
  {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    DirectX::XMFLOAT4 color{};

    [[nodiscard]] bool Contains(float _xPixels, float _yPixels) const noexcept
    {
      return _xPixels >= left && _xPixels < left + width && _yPixels >= top && _yPixels < top + height;
    }
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
    // Where each enabled button is, for clicks.
    std::vector<std::pair<Rect, Action>> actions;
    // The minimap's drawing area, the map's square; empty when there is no minimap.
    Rect minimap;
    float mapSizeMeters = 0.0f;

    // Whether a point in back-buffer pixels is on a panel, where a click belongs to the HUD.
    [[nodiscard]] bool Covers(float _xPixels, float _yPixels) const noexcept;
    // The enabled button under a point, if any.
    [[nodiscard]] std::optional<Action> ActionAt(float _xPixels, float _yPixels) const noexcept;
    // The point on the map under a point on the minimap, if it is on the minimap.
    [[nodiscard]] std::optional<PlanePosition> MapPointAt(float _xPixels, float _yPixels) const noexcept;
    // Where a point on the map shows on the minimap, in pixels.
    [[nodiscard]] DirectX::XMFLOAT2 MinimapPixelOf(PlanePosition _point) const noexcept;
  };

  // The content for _player: its Ore and income from the newest snapshot, a description of _selected, by design name
  // from the snapshot's designs, the buttons the selection offers, and the minimap's marks. _placing is the structure
  // being placed, if any.
  [[nodiscard]] static Content Describe(const Snapshot& _newest, std::span<const EntityView> _entities, std::span<const EntityId> _selected,
                                        std::optional<StructureKind> _placing = std::nullopt);

  // Where everything goes on a back buffer of this size. _view is the ground the camera shows, its corners in order,
  // outlined on the minimap; empty when the camera sees past the horizon.
  [[nodiscard]] static Layout Lay(const Content& _content, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                                  std::span<const PlanePosition> _view = {});

  // The scale from reference units to pixels: the largest at which the whole reference frame fits (ADR-006).
  [[nodiscard]] static float Scale(std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept;
};

// _value with a comma between each group of three digits, such as "12,000".
[[nodiscard]] std::string WithThousands(std::int64_t _value);
} // namespace Outpost
