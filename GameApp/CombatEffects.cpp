#include "pch.h"
#include "CombatEffects.h"

#include <algorithm>
#include <cmath>

namespace
{
using Outpost::CombatEffects;

// Placeholder looks, chosen to read clearly from the RTS camera (design §11). Presentation, not tuning, so they live
// here rather than in the tuning data.

// Which weapon draws a beam, by the tuning data's numbering: the Lance. Every other weapon fires tracers.
constexpr Outpost::WeaponId BEAM_WEAPON{2};

// Effects sit above the ships they belong to, so the ships do not hide them.
constexpr float EFFECT_HEIGHT_METERS = 6.0f;
constexpr float EXPLOSION_HEIGHT_METERS = 7.0f;

constexpr double FLASH_SECONDS = 0.08;
constexpr float TRACER_FLASH_RADIUS_METERS = 3.0f;
constexpr float BEAM_FLASH_RADIUS_METERS = 5.0f;
constexpr DirectX::XMFLOAT4 FLASH_COLOR{1.0f, 0.95f, 0.7f, 1.0f};

// A tracer is a short streak that crosses from the gun to the target, then a spark where it lands.
constexpr double TRACER_FLIGHT_SECONDS = 0.12;
constexpr float TRACER_LENGTH_METERS = 14.0f;
constexpr float TRACER_WIDTH_METERS = 2.0f;
constexpr DirectX::XMFLOAT4 TRACER_COLOR{1.0f, 0.85f, 0.35f, 1.0f};

// A beam joins the gun and the target at once and narrows as it fades.
constexpr double BEAM_SECONDS = 0.25;
constexpr float BEAM_WIDTH_METERS = 3.0f;
constexpr DirectX::XMFLOAT4 BEAM_COLOR{0.55f, 0.9f, 1.0f, 1.0f};

constexpr double SPARK_SECONDS = 0.12;
constexpr float SPARK_RADIUS_METERS = 4.0f;
constexpr DirectX::XMFLOAT4 SPARK_COLOR{1.0f, 0.7f, 0.3f, 1.0f};

// An explosion is a fireball that swells and darkens, and a shock ring that runs out past it, both sized by what blew.
constexpr double EXPLOSION_SECONDS = 0.9;
constexpr float FIREBALL_START_RADII = 0.6f;
constexpr float FIREBALL_END_RADII = 1.6f;
constexpr float SHOCK_START_RADII = 0.5f;
constexpr float SHOCK_END_RADII = 3.0f;
constexpr DirectX::XMFLOAT4 FIREBALL_COLOR{1.0f, 0.75f, 0.3f, 1.0f};
constexpr DirectX::XMFLOAT4 SHOCK_COLOR{1.0f, 0.55f, 0.2f, 1.0f};

// _color darkened toward black by how far through its life an effect is, from 0 to 1.
DirectX::XMFLOAT4 Faded(const DirectX::XMFLOAT4& _color, double _progress) noexcept
{
  const auto keep = static_cast<float>(std::clamp(1.0 - _progress, 0.0, 1.0));
  return {_color.x * keep, _color.y * keep, _color.z * keep, _color.w};
}

Outpost::PlanePosition Lerp(Outpost::PlanePosition _from, Outpost::PlanePosition _to, float _fraction) noexcept
{
  return {.xMeters = _from.xMeters + ((_to.xMeters - _from.xMeters) * _fraction),
          .zMeters = _from.zMeters + ((_to.zMeters - _from.zMeters) * _fraction)};
}

CombatEffects::Draw Disc(Outpost::PlanePosition _at, float _radiusMeters, float _heightMeters, const DirectX::XMFLOAT4& _color)
{
  return {.shape = CombatEffects::Shape::Disc,
          .from = _at,
          .to = _at,
          .radiusMeters = _radiusMeters,
          .heightMeters = _heightMeters,
          .color = _color};
}
} // namespace

Outpost::CombatEffects::CombatEffects(std::uint32_t _ticksPerSecond)
  : m_ticksPerSecond(static_cast<double>(_ticksPerSecond))
{
}

void Outpost::CombatEffects::Receive(const Snapshot& _snapshot)
{
  const double start = static_cast<double>(_snapshot.tick) - 1.0;
  for (const ShotView& shot : _snapshot.shots)
    m_effects.push_back({.kind = shot.weapon == BEAM_WEAPON ? Kind::Beam : Kind::Tracer,
                         .startTick = start,
                         .shooter = shot.shooter,
                         .from = shot.from,
                         .to = shot.to});
  for (const DestroyedView& destroyed : _snapshot.destroyed)
    m_effects.push_back({.kind = Kind::Explosion,
                         .startTick = start,
                         .from = destroyed.position,
                         .to = destroyed.position,
                         .radiusMeters = destroyed.radiusMeters});
}

std::vector<Outpost::CombatEffects::Draw> Outpost::CombatEffects::At(double _viewTick, const MuzzleLocator& _muzzle)
{
  const auto lifetime = [](Kind _kind)
  {
    switch (_kind)
    {
    case Kind::Tracer:
      return TRACER_FLIGHT_SECONDS + SPARK_SECONDS;
    case Kind::Beam:
      return std::max(BEAM_SECONDS, SPARK_SECONDS);
    case Kind::Explosion:
    default:
      return EXPLOSION_SECONDS;
    }
  };
  std::erase_if(m_effects, [&](const Effect& _effect) { return Seconds(_viewTick - _effect.startTick) > lifetime(_effect.kind); });

  std::vector<Draw> draws;
  for (const Effect& effect : m_effects)
  {
    if (_viewTick < effect.startTick)
      continue;
    if (effect.kind == Kind::Explosion)
    {
      AddExplosion(effect, _viewTick, draws);
      continue;
    }
    Effect shot = effect;
    if (_muzzle)
      shot.from = _muzzle(effect.shooter, effect.to).value_or(effect.from);
    AddShot(shot, _viewTick, draws);
  }
  return draws;
}

void Outpost::CombatEffects::AddShot(const Effect& _effect, double _tick, std::vector<Draw>& _draws) const
{
  const double elapsed = Seconds(_tick - _effect.startTick);
  const bool beam = _effect.kind == Kind::Beam;
  if (elapsed < FLASH_SECONDS)
    _draws.push_back(Disc(_effect.from, beam ? BEAM_FLASH_RADIUS_METERS : TRACER_FLASH_RADIUS_METERS, EFFECT_HEIGHT_METERS,
                          Faded(FLASH_COLOR, elapsed / FLASH_SECONDS)));

  const float lengthMeters = std::hypot(_effect.to.xMeters - _effect.from.xMeters, _effect.to.zMeters - _effect.from.zMeters);
  double sparkStart = 0.0;
  if (beam)
  {
    if (elapsed < BEAM_SECONDS)
    {
      const double progress = elapsed / BEAM_SECONDS;
      _draws.push_back({.shape = Shape::Band,
                        .from = _effect.from,
                        .to = _effect.to,
                        .widthMeters = BEAM_WIDTH_METERS * static_cast<float>(1.0 - progress),
                        .heightMeters = EFFECT_HEIGHT_METERS,
                        .color = Faded(BEAM_COLOR, progress * 0.5)});
    }
  }
  else
  {
    sparkStart = TRACER_FLIGHT_SECONDS;
    if (elapsed < TRACER_FLIGHT_SECONDS && lengthMeters > 0.0f)
    {
      // The streak's head runs from the gun to the target; its tail follows a streak's length behind, never behind the gun.
      const auto head = static_cast<float>(elapsed / TRACER_FLIGHT_SECONDS);
      const float tail = std::max(0.0f, head - (TRACER_LENGTH_METERS / lengthMeters));
      _draws.push_back({.shape = Shape::Band,
                        .from = Lerp(_effect.from, _effect.to, tail),
                        .to = Lerp(_effect.from, _effect.to, head),
                        .widthMeters = TRACER_WIDTH_METERS,
                        .heightMeters = EFFECT_HEIGHT_METERS,
                        .color = TRACER_COLOR});
    }
  }

  const double sparkElapsed = elapsed - sparkStart;
  if (sparkElapsed >= 0.0 && sparkElapsed < SPARK_SECONDS)
    _draws.push_back(Disc(_effect.to, SPARK_RADIUS_METERS, EFFECT_HEIGHT_METERS, Faded(SPARK_COLOR, sparkElapsed / SPARK_SECONDS)));
}

void Outpost::CombatEffects::AddExplosion(const Effect& _effect, double _tick, std::vector<Draw>& _draws) const
{
  const double progress = Seconds(_tick - _effect.startTick) / EXPLOSION_SECONDS;
  const auto grow = [progress](float _start, float _end) { return _start + ((_end - _start) * static_cast<float>(progress)); };
  const float radius = _effect.radiusMeters;
  _draws.push_back(
    Disc(_effect.from, radius * grow(FIREBALL_START_RADII, FIREBALL_END_RADII), EXPLOSION_HEIGHT_METERS, Faded(FIREBALL_COLOR, progress)));
  _draws.push_back({.shape = Shape::Ring,
                    .from = _effect.from,
                    .to = _effect.from,
                    .radiusMeters = radius * grow(SHOCK_START_RADII, SHOCK_END_RADII),
                    .heightMeters = EXPLOSION_HEIGHT_METERS,
                    .color = Faded(SHOCK_COLOR, progress)});
}
