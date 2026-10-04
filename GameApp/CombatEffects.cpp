#include "pch.h"
#include "CombatEffects.h"

#include <algorithm>
#include <cmath>

namespace
{
using Outpost::CombatEffects;

// Placeholder looks, chosen to read clearly from the RTS camera (design §11). Presentation, not tuning, so they live
// here rather than in the tuning data.

// Effects sit above the ships they belong to, so the ships do not hide them.
constexpr float EFFECT_HEIGHT_METERS = 6.0f;

constexpr double FLASH_SECONDS = 0.08;
constexpr float TRACER_FLASH_RADIUS_METERS = 3.0f;
constexpr float BEAM_FLASH_RADIUS_METERS = 5.0f;
constexpr DirectX::XMFLOAT4 FLASH_COLOR{1.0f, 0.95f, 0.7f, 1.0f};

// A tracer is a short streak that crosses from the gun to the target, then a spark where it lands.
constexpr double TRACER_FLIGHT_SECONDS = 0.12;
constexpr float TRACER_LENGTH_METERS = 14.0f;
constexpr float TRACER_WIDTH_METERS = 2.0f;
constexpr DirectX::XMFLOAT4 TRACER_COLOR{1.0f, 0.85f, 0.35f, 1.0f};

// A beam joins the gun and the target at once and narrows as it fades. Its color is its shooter's side's, from
// GameClient; this one is for a shooter it cannot place.
constexpr double BEAM_SECONDS = 0.25;
constexpr float BEAM_WIDTH_METERS = 4.0f;
constexpr DirectX::XMFLOAT4 BEAM_COLOR{0.55f, 0.9f, 1.0f, 1.0f};

// A slug joins the gun and the target at once, a thin white line that fades where it stands, with a wide flash at both
// ends: a heavy hit, rarely.
constexpr double SLUG_SECONDS = 0.4;
constexpr float SLUG_WIDTH_METERS = 1.5f;
constexpr float SLUG_FLASH_RADIUS_METERS = 7.0f;
constexpr DirectX::XMFLOAT4 SLUG_COLOR{0.9f, 0.95f, 1.0f, 1.0f};

constexpr double SPARK_SECONDS = 0.12;
// A splash weapon's hit also throws a ring that runs out to its splash radius, so the player sees what it reached.
constexpr double BLAST_SECONDS = 0.3;
constexpr DirectX::XMFLOAT4 BLAST_COLOR{1.0f, 0.6f, 0.25f, 1.0f};
constexpr float SPARK_RADIUS_METERS = 4.0f;
constexpr DirectX::XMFLOAT4 SPARK_COLOR{1.0f, 0.7f, 0.3f, 1.0f};

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

CombatEffects::CombatEffects(std::uint32_t _ticksPerSecond, std::vector<WeaponShot> _shots)
  : m_ticksPerSecond(static_cast<double>(_ticksPerSecond)),
    m_shots(std::move(_shots))
{
}

void CombatEffects::Receive(const Snapshot& _snapshot)
{
  const double start = static_cast<double>(_snapshot.tick) - 1.0;
  for (const ShotView& shot : _snapshot.shots)
  {
    const auto look = std::ranges::find(m_shots, shot.weapon, &WeaponShot::weapon);
    m_effects.push_back({.look = look != m_shots.end() ? look->look : ShotLook::Tracer,
                         .startTick = start,
                         .shooter = shot.shooter,
                         .from = shot.from,
                         .to = shot.to,
                         .radiusMeters = shot.splashRadiusMeters,
                         .gun = shot.gun});
  }
}

std::vector<CombatEffects::Draw> CombatEffects::At(double _viewTick, const MuzzleLocator& _muzzle, const BeamTint& _tint)
{
  const auto lifetime = [](ShotLook _look)
  {
    if (_look == ShotLook::Beam)
      return std::max(BEAM_SECONDS, SPARK_SECONDS);
    if (_look == ShotLook::Slug)
      return std::max({SLUG_SECONDS, SPARK_SECONDS, BLAST_SECONDS});
    return TRACER_FLIGHT_SECONDS + std::max(SPARK_SECONDS, BLAST_SECONDS);
  };
  std::erase_if(m_effects, [&](const Effect& _effect) { return Seconds(_viewTick - _effect.startTick) > lifetime(_effect.look); });

  std::vector<Draw> draws;
  for (const Effect& effect : m_effects)
  {
    if (_viewTick < effect.startTick)
      continue;
    Effect shot = effect;
    if (_muzzle)
      shot.from = _muzzle(effect.shooter, effect.to, effect.gun).value_or(effect.from);
    const DirectX::XMFLOAT4 beamColor = shot.look == ShotLook::Beam && _tint ? _tint(shot.shooter).value_or(BEAM_COLOR) : BEAM_COLOR;
    AddShot(shot, _viewTick, beamColor, draws);
  }
  return draws;
}

void CombatEffects::AddShot(const Effect& _effect, double _tick, const DirectX::XMFLOAT4& _beamColor, std::vector<Draw>& _draws) const
{
  const double elapsed = Seconds(_tick - _effect.startTick);
  const float flashRadiusMeters = _effect.look == ShotLook::Beam   ? BEAM_FLASH_RADIUS_METERS
                                  : _effect.look == ShotLook::Slug ? SLUG_FLASH_RADIUS_METERS
                                                                   : TRACER_FLASH_RADIUS_METERS;
  if (elapsed < FLASH_SECONDS)
    _draws.push_back(Disc(_effect.from, flashRadiusMeters, EFFECT_HEIGHT_METERS, Faded(FLASH_COLOR, elapsed / FLASH_SECONDS)));

  const float lengthMeters = std::hypot(_effect.to.xMeters - _effect.from.xMeters, _effect.to.zMeters - _effect.from.zMeters);
  double sparkStart = 0.0;
  if (_effect.look == ShotLook::Slug)
  {
    if (elapsed < SLUG_SECONDS)
    {
      const double progress = elapsed / SLUG_SECONDS;
      _draws.push_back({.shape = Shape::Band,
                        .from = _effect.from,
                        .to = _effect.to,
                        .widthMeters = SLUG_WIDTH_METERS,
                        .heightMeters = EFFECT_HEIGHT_METERS,
                        .color = Faded(SLUG_COLOR, progress)});
    }
    if (elapsed < FLASH_SECONDS)
      _draws.push_back(Disc(_effect.to, SLUG_FLASH_RADIUS_METERS, EFFECT_HEIGHT_METERS, Faded(FLASH_COLOR, elapsed / FLASH_SECONDS)));
  }
  else if (_effect.look == ShotLook::Beam)
  {
    if (elapsed < BEAM_SECONDS)
    {
      const double progress = elapsed / BEAM_SECONDS;
      _draws.push_back({.shape = Shape::Band,
                        .from = _effect.from,
                        .to = _effect.to,
                        .widthMeters = BEAM_WIDTH_METERS * static_cast<float>(1.0 - progress),
                        .heightMeters = EFFECT_HEIGHT_METERS,
                        .color = Faded(_beamColor, progress * 0.5)});
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
  if (_effect.radiusMeters > 0.0f && sparkElapsed >= 0.0 && sparkElapsed < BLAST_SECONDS)
  {
    const double progress = sparkElapsed / BLAST_SECONDS;
    _draws.push_back({.shape = Shape::Ring,
                      .from = _effect.to,
                      .to = _effect.to,
                      .radiusMeters = _effect.radiusMeters * static_cast<float>(std::sqrt(progress)),
                      .heightMeters = EFFECT_HEIGHT_METERS,
                      .color = Faded(BLAST_COLOR, progress)});
  }
}
