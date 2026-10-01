#include "pch.h"
#include "Picking.h"

#include <algorithm>
#include <cmath>

namespace
{
bool IsShipOf(const Outpost::EntityView& _entity, Outpost::PlayerId _player) noexcept
{
  return _entity.kind == Outpost::EntityKind::Ship && _entity.owner == _player;
}

bool IsOnViewport(DirectX::XMFLOAT2 _pixel, const Outpost::Viewport& _viewport) noexcept
{
  return _pixel.x >= 0.0f && _pixel.y >= 0.0f && _pixel.x <= static_cast<float>(_viewport.widthPixels) &&
         _pixel.y <= static_cast<float>(_viewport.heightPixels);
}
} // namespace

Outpost::ScreenRect Outpost::ScreenRect::Between(float _x0, float _y0, float _x1, float _y1) noexcept
{
  return {.left = std::min(_x0, _x1), .top = std::min(_y0, _y1), .right = std::max(_x0, _x1), .bottom = std::max(_y0, _y1)};
}

std::optional<Outpost::EntityId> Outpost::PickShip(std::span<const EntityView> _entities, const Camera& _camera, const Viewport& _viewport,
                                                   DirectX::XMFLOAT2 _cursor, const std::function<bool(const EntityView&)>& _eligible)
{
  return PickEntity(_entities, _camera, _viewport, _cursor,
                    [&_eligible](const EntityView& _entity) { return _entity.kind == EntityKind::Ship && _eligible(_entity); });
}

std::optional<Outpost::EntityId> Outpost::PickEntity(std::span<const EntityView> _entities, const Camera& _camera,
                                                     const Viewport& _viewport, DirectX::XMFLOAT2 _cursor,
                                                     const std::function<bool(const EntityView&)>& _eligible)
{
  // Near the focus a meter is this many pixels; a footprint is measured with it, which is close enough for picking.
  const float pixelsPerMeter = static_cast<float>(_viewport.widthPixels) / _camera.ViewWidthMeters();
  std::optional<EntityId> picked;
  float nearestPixels = 0.0f;
  for (const EntityView& entity : _entities)
  {
    if (!_eligible(entity))
      continue;
    const std::optional<DirectX::XMFLOAT2> pixel = _camera.PixelOf(entity.position, _viewport);
    if (!pixel.has_value())
      continue;
    const float distance = std::hypot(pixel->x - _cursor.x, pixel->y - _cursor.y);
    const float reach = std::max(PICK_TOLERANCE_PIXELS, entity.radiusMeters * pixelsPerMeter);
    if (distance <= reach && (!picked.has_value() || distance < nearestPixels))
    {
      picked = entity.id;
      nearestPixels = distance;
    }
  }
  return picked;
}

std::vector<Outpost::EntityId> Outpost::ShipsInBox(std::span<const EntityView> _entities, const Camera& _camera, const Viewport& _viewport,
                                                   const ScreenRect& _box, PlayerId _player)
{
  std::vector<EntityId> ships;
  for (const EntityView& entity : _entities)
  {
    if (!IsShipOf(entity, _player))
      continue;
    const std::optional<DirectX::XMFLOAT2> pixel = _camera.PixelOf(entity.position, _viewport);
    if (pixel.has_value() && _box.Contains(pixel->x, pixel->y))
      ships.push_back(entity.id);
  }
  return ships;
}

std::vector<Outpost::EntityId> Outpost::VisibleShipsLike(std::span<const EntityView> _entities, const Camera& _camera,
                                                         const Viewport& _viewport, const EntityView& _like, PlayerId _player)
{
  std::vector<EntityId> ships;
  for (const EntityView& entity : _entities)
  {
    if (!IsShipOf(entity, _player) || entity.design != _like.design || entity.hull != _like.hull)
      continue;
    const std::optional<DirectX::XMFLOAT2> pixel = _camera.PixelOf(entity.position, _viewport);
    if (pixel.has_value() && IsOnViewport(*pixel, _viewport))
      ships.push_back(entity.id);
  }
  return ships;
}