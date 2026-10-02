#include "pch.h"
#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>

namespace
{
using Glow = Neuron::GlowPipeline::Glow;
using Outpost::ParticleSystem;

// Placeholder looks, after DeepSpaceOutpost's Particle::SetupParticles, and presentation rather than tuning (design §11).
// The colors are linear and carry DeepSpaceOutpost's brightness, its 90/255 alpha, since a glow's color includes its
// brightness (ADR-019). Its fireball and debris were red; here they run from a white-yellow flash to amber, so that a
// blast is never read as the Tarkan's red (owner, 2026-10-02, ADR-028). Its smoke stays gray. Its sizes and speeds were
// in its own units; here they are shares of a blast's reach, so that a blast is as big as what blew up.
struct Look
{
  double lifeSeconds = 0.0;
  // How fast a particle slows, per second: its speed falls as e^(-friction * age).
  float friction = 0.0f;
  DirectX::XMFLOAT3 color1{};
  DirectX::XMFLOAT3 color2{};
};

constexpr Look CORE{.lifeSeconds = 2.0, .friction = 0.2f, .color1 = {0.353f, 0.31f, 0.19f}, .color2 = {0.337f, 0.2f, 0.07f}};
constexpr Look DEBRIS{.lifeSeconds = 6.0, .friction = 0.2f, .color1 = {0.22f, 0.13f, 0.04f}, .color2 = {0.337f, 0.22f, 0.08f}};
// DeepSpaceOutpost's rocket trail, the smoke its debris leaves.
constexpr Look TRAIL{.lifeSeconds = 2.0, .friction = 0.6f, .color1 = {0.076f, 0.076f, 0.076f}, .color2 = {0.204f, 0.204f, 0.204f}};

// A particle keeps its full brightness for this share of its life, then fades linearly to nothing.
constexpr double FADE_START_SHARE = 0.75;

// Debris may leave a puff of smoke every TRAIL_STEP_SECONDS, DeepSpaceOutpost's server advance, with this chance: half its
// size, at a fifth of its speed.
constexpr double TRAIL_STEP_SECONDS = 0.1;
constexpr float TRAIL_CHANCE = 0.3f;
constexpr float TRAIL_SIZE_SHARE = 0.5f;
constexpr float TRAIL_SPEED_SHARE = 0.2f;

// A blast reaches this many times the footprint of what blew up: DeepSpaceOutpost's Bang range.
constexpr float BLAST_REACH_RADII = 1.5f;
// The fireball: so many puffs per meter of footprint, and up to as many again, each a share of the reach across, thrown
// at up to a share of the reach a second across the plane and half that up or down.
constexpr float CORES_PER_METER = 1.0f;
constexpr float CORE_MIN_SIZE_REACHES = 0.18f;
constexpr float CORE_MAX_SIZE_REACHES = 0.27f;
constexpr float CORE_SPEED_REACHES = 0.67f;
// The debris: a few, and more for something bigger, smaller and faster than the fireball.
constexpr float DEBRIS_BASE = 3.0f;
constexpr float DEBRIS_PER_METER = 0.25f;
constexpr float DEBRIS_MIN_SIZE_REACHES = 0.08f;
constexpr float DEBRIS_MAX_SIZE_REACHES = 0.12f;
constexpr float DEBRIS_SPEED_REACHES = 1.0f;
// A structure also throws out a flash of fast puffs, one for every two meters of its footprint.
constexpr float FLASH_PER_METER = 0.5f;
constexpr float FLASH_SIZE_REACHES = 0.15f;
constexpr float FLASH_SPEED_REACHES = 2.5f;
// Across the plane, which the camera looks down on, a blast spreads wider than up and down.
constexpr float VERTICAL_SPEED_SHARE = 0.5f;

const Look& LookOf(ParticleSystem::Kind _kind) noexcept
{
  return _kind == ParticleSystem::Kind::ExplosionDebris ? DEBRIS : CORE;
}

DirectX::XMFLOAT4 Mix(const Look& _look, float _share) noexcept
{
  const float keep = 1.0f - _share;
  return {(_look.color1.x * keep) + (_look.color2.x * _share), (_look.color1.y * keep) + (_look.color2.y * _share),
          (_look.color1.z * keep) + (_look.color2.z * _share), 1.0f};
}

// How far along its velocity a particle has come after _ageSeconds: the integral of e^(-friction * t).
float Travel(float _friction, double _ageSeconds) noexcept
{
  if (_friction <= 0.0f)
    return static_cast<float>(_ageSeconds);
  return static_cast<float>((1.0 - std::exp(-_friction * _ageSeconds)) / _friction);
}

DirectX::XMFLOAT3 Along(const DirectX::XMFLOAT3& _from, const DirectX::XMFLOAT3& _velocity, float _seconds) noexcept
{
  return {_from.x + (_velocity.x * _seconds), _from.y + (_velocity.y * _seconds), _from.z + (_velocity.z * _seconds)};
}

DirectX::XMFLOAT3 Scaled(const DirectX::XMFLOAT3& _vector, float _scale) noexcept
{
  return {_vector.x * _scale, _vector.y * _scale, _vector.z * _scale};
}

// The share of its brightness a particle of _look keeps at _ageSeconds.
float Brightness(const Look& _look, double _ageSeconds) noexcept
{
  const double fadeStart = _look.lifeSeconds * FADE_START_SHARE;
  if (_ageSeconds <= fadeStart)
    return 1.0f;
  return static_cast<float>(std::clamp(1.0 - ((_ageSeconds - fadeStart) / (_look.lifeSeconds - fadeStart)), 0.0, 1.0));
}

Glow MakeGlow(const DirectX::XMFLOAT3& _position, float _radiusMeters, const DirectX::XMFLOAT4& _color, float _brightness) noexcept
{
  return {.position = _position,
          .radiusMeters = _radiusMeters,
          .color = {_color.x * _brightness, _color.y * _brightness, _color.z * _brightness, _color.w}};
}

// A velocity of up to _speed across the plane and VERTICAL_SPEED_SHARE of it up or down. Each draw is its own statement,
// as is every draw in AddBlast, so the order they are drawn in is fixed and a seed gives the same blast on any compiler.
DirectX::XMFLOAT3 Scatter(Outpost::EffectRandom& _random, float _speed) noexcept
{
  const float x = _random.Signed(_speed);
  const float y = _random.Signed(_speed * VERTICAL_SPEED_SHARE);
  const float z = _random.Signed(_speed);
  return {x, y, z};
}
} // namespace

ParticleSystem::ParticleSystem(std::uint32_t _ticksPerSecond)
  : m_ticksPerSecond(static_cast<double>(_ticksPerSecond))
{
}

void ParticleSystem::Add(Kind _kind, const DirectX::XMFLOAT3& _position, const DirectX::XMFLOAT3& _velocity, float _radiusMeters,
                         float _colorShare, double _birthTick, std::uint64_t _seed)
{
  if (m_particles.size() >= MAX_PARTICLES)
    return;
  m_particles.push_back({.kind = _kind,
                         .birthTick = _birthTick,
                         .position = _position,
                         .velocity = _velocity,
                         .radiusMeters = _radiusMeters,
                         .color = Mix(LookOf(_kind), std::clamp(_colorShare, 0.0f, 1.0f)),
                         .seed = _seed});
}

void ParticleSystem::AddBlast(const DirectX::XMFLOAT3& _center, float _radiusMeters, EntityKind _kind, double _startTick,
                              std::uint64_t _seed)
{
  EffectRandom random(_seed);
  const float reach = BLAST_REACH_RADII * _radiusMeters;

  // As Bang does: a count, and up to as many again.
  const auto cores = static_cast<int>(CORES_PER_METER * _radiusMeters);
  const int coreCount = cores + static_cast<int>(random.Unit() * static_cast<float>(cores));
  for (int i = 0; i < coreCount; ++i)
  {
    const DirectX::XMFLOAT3 velocity = Scatter(random, CORE_SPEED_REACHES * reach);
    const float size = random.Between(CORE_MIN_SIZE_REACHES, CORE_MAX_SIZE_REACHES) * reach;
    const float colorShare = random.Unit();
    Add(Kind::ExplosionCore, _center, velocity, size, colorShare, _startTick, random.Next());
  }

  const int debrisCount = std::max(1, static_cast<int>(DEBRIS_BASE + (DEBRIS_PER_METER * _radiusMeters)));
  for (int i = 0; i < debrisCount; ++i)
  {
    const DirectX::XMFLOAT3 velocity = Scatter(random, DEBRIS_SPEED_REACHES * reach);
    const float size = random.Between(DEBRIS_MIN_SIZE_REACHES, DEBRIS_MAX_SIZE_REACHES) * reach;
    const float colorShare = random.Unit();
    Add(Kind::ExplosionDebris, _center, velocity, size, colorShare, _startTick, random.Next());
  }

  if (_kind != EntityKind::Structure)
    return;
  const auto flashCount = static_cast<int>(FLASH_PER_METER * _radiusMeters);
  for (int i = 0; i < flashCount; ++i)
  {
    const DirectX::XMFLOAT3 velocity = Scatter(random, FLASH_SPEED_REACHES * reach);
    const float colorShare = random.Unit();
    Add(Kind::ExplosionCore, _center, velocity, FLASH_SIZE_REACHES * reach, colorShare, _startTick, random.Next());
  }
}

void ParticleSystem::At(double _viewTick, std::vector<Glow>& _glows)
{
  // Debris stays until the last puff of its trail has faded.
  const auto lastSeconds = [](Kind _kind)
  { return _kind == Kind::ExplosionDebris ? DEBRIS.lifeSeconds + TRAIL.lifeSeconds : LookOf(_kind).lifeSeconds; };
  std::erase_if(m_particles,
                [&](const Particle& _particle) { return Seconds(_viewTick - _particle.birthTick) > lastSeconds(_particle.kind); });

  for (const Particle& particle : m_particles)
  {
    const double age = Seconds(_viewTick - particle.birthTick);
    if (age < 0.0)
      continue;
    const Look& look = LookOf(particle.kind);
    if (age <= look.lifeSeconds)
    {
      const DirectX::XMFLOAT3 position = Along(particle.position, particle.velocity, Travel(look.friction, age));
      _glows.push_back(MakeGlow(position, particle.radiusMeters, particle.color, Brightness(look, age)));
    }
    if (particle.kind == Kind::ExplosionDebris)
      AddTrail(particle, age, _glows);
  }
}

void ParticleSystem::AddTrail(const Particle& _debris, double _ageSeconds, std::vector<Glow>& _glows) const
{
  // The puffs still showing were left from TRAIL's life ago up to now, and only while the debris itself flew. None is
  // left at its birth.
  const auto first =
    std::max<std::int64_t>(1, static_cast<std::int64_t>(std::ceil((_ageSeconds - TRAIL.lifeSeconds) / TRAIL_STEP_SECONDS)));
  const auto last = static_cast<std::int64_t>(std::floor(std::min(_ageSeconds, DEBRIS.lifeSeconds) / TRAIL_STEP_SECONDS));
  for (std::int64_t step = first; step <= last; ++step)
  {
    // Whether a step leaves a puff is drawn from the debris's seed and the step, so it is the same in every frame.
    EffectRandom random(_debris.seed + static_cast<std::uint64_t>(step));
    if (random.Unit() >= TRAIL_CHANCE)
      continue;
    const double leftAt = static_cast<double>(step) * TRAIL_STEP_SECONDS;
    const double puffAge = _ageSeconds - leftAt;
    if (puffAge < 0.0 || puffAge > TRAIL.lifeSeconds)
      continue;
    const DirectX::XMFLOAT3 from = Along(_debris.position, _debris.velocity, Travel(DEBRIS.friction, leftAt));
    const float slowed = static_cast<float>(std::exp(-DEBRIS.friction * leftAt));
    const DirectX::XMFLOAT3 velocity = Scaled(_debris.velocity, slowed * TRAIL_SPEED_SHARE);
    const DirectX::XMFLOAT3 position = Along(from, velocity, Travel(TRAIL.friction, puffAge));
    _glows.push_back(MakeGlow(position, _debris.radiusMeters * TRAIL_SIZE_SHARE, Mix(TRAIL, random.Unit()), Brightness(TRAIL, puffAge)));
  }
}
