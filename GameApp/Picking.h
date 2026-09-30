#pragma once

namespace Outpost
{
// A rectangle on the viewport, in pixels from its top-left corner, with left <= right and top <= bottom.
struct ScreenRect
{
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  // The rectangle between two corners given in any order.
  [[nodiscard]] static ScreenRect Between(float _x0, float _y0, float _x1, float _y1) noexcept;

  [[nodiscard]] bool Contains(float _x, float _y) const noexcept
  {
    return _x >= left && _x <= right && _y >= top && _y <= bottom;
  }
};

// Which ships a click or a box takes, from where each ship shows on the screen (task 2.6). Pure math over the camera and
// the snapshot's entities, so it is tested without a window.

// A click this close to a ship takes it, however small the ship shows.
inline constexpr float PICK_TOLERANCE_PIXELS = 12.0f;

// The ship under the cursor that _eligible accepts: the nearest on screen to _cursor among those whose footprint, or
// PICK_TOLERANCE_PIXELS if that is larger, reaches it. Nothing when none does.
[[nodiscard]] std::optional<EntityId> PickShip(std::span<const EntityView> _entities, const Camera& _camera, const Viewport& _viewport,
                                               DirectX::XMFLOAT2 _cursor, const std::function<bool(const EntityView&)>& _eligible);

// _player's ships that show inside _box.
[[nodiscard]] std::vector<EntityId> ShipsInBox(std::span<const EntityView> _entities, const Camera& _camera, const Viewport& _viewport,
                                               const ScreenRect& _box, PlayerId _player);

// _player's ships on screen of the same design and hull as _like (design §9's double-click).
[[nodiscard]] std::vector<EntityId> VisibleShipsLike(std::span<const EntityView> _entities, const Camera& _camera,
                                                     const Viewport& _viewport, const EntityView& _like, PlayerId _player);
} // namespace Outpost
