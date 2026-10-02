#include "pch.h"

#include <algorithm>
#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
using Glow = Neuron::GlowPipeline::Glow;
using Kind = Outpost::ParticleSystem::Kind;

constexpr std::uint32_t TICKS_PER_SECOND = 20;
constexpr float TOLERANCE = 1e-3f;
constexpr DirectX::XMFLOAT3 ORIGIN{0.0f, 0.0f, 0.0f};
constexpr std::uint64_t SEED = 42;

double Ticks(double _seconds)
{
  return _seconds * TICKS_PER_SECOND;
}

std::vector<Glow> GlowsAt(Outpost::ParticleSystem& _particles, double _seconds)
{
  std::vector<Glow> glows;
  _particles.At(Ticks(_seconds), glows);
  return glows;
}

float DistanceFromOrigin(const Glow& _glow)
{
  return std::sqrt((_glow.position.x * _glow.position.x) + (_glow.position.y * _glow.position.y) + (_glow.position.z * _glow.position.z));
}

float Reach(const std::vector<Glow>& _glows)
{
  float farthest = 0.0f;
  for (const Glow& glow : _glows)
    farthest = std::max(farthest, DistanceFromOrigin(glow));
  return farthest;
}

bool SameGlows(const std::vector<Glow>& _first, const std::vector<Glow>& _second)
{
  return std::ranges::equal(_first, _second,
                            [](const Glow& _a, const Glow& _b)
                            {
                              return _a.position.x == _b.position.x && _a.position.y == _b.position.y && _a.position.z == _b.position.z &&
                                     _a.radiusMeters == _b.radiusMeters && _a.color.x == _b.color.x;
                            });
}
} // namespace

TEST_CLASS(ParticleSystemTests)
{
public:
  // ADR-026: a blast plays when the view reaches its start, and every particle starts at the center.
  TEST_METHOD(ABlastWaitsForTheView)
  {
    Outpost::ParticleSystem particles(TICKS_PER_SECOND);
    particles.AddBlast(ORIGIN, 8.0f, Outpost::EntityKind::Ship, Ticks(1.0), SEED);
    Assert::IsTrue(GlowsAt(particles, 0.9).empty());
    const std::vector<Glow> glows = GlowsAt(particles, 1.0);
    Assert::IsFalse(glows.empty());
    Assert::AreEqual(0.0f, Reach(glows), TOLERANCE);
  }

  TEST_METHOD(TheSameSeedGivesTheSameBlast)
  {
    Outpost::ParticleSystem first(TICKS_PER_SECOND);
    Outpost::ParticleSystem second(TICKS_PER_SECOND);
    Outpost::ParticleSystem other(TICKS_PER_SECOND);
    first.AddBlast(ORIGIN, 14.0f, Outpost::EntityKind::Ship, 0.0, SEED);
    second.AddBlast(ORIGIN, 14.0f, Outpost::EntityKind::Ship, 0.0, SEED);
    other.AddBlast(ORIGIN, 14.0f, Outpost::EntityKind::Ship, 0.0, SEED + 1);
    const std::vector<Glow> firstGlows = GlowsAt(first, 1.5);
    Assert::IsTrue(SameGlows(firstGlows, GlowsAt(second, 1.5)));
    Assert::IsFalse(SameGlows(firstGlows, GlowsAt(other, 1.5)));
  }

  // DeepSpaceOutpost's friction: a particle's speed falls as e^(-friction * age), so it covers v (1 - e^(-kt)) / k.
  TEST_METHOD(AParticleSlowsUnderFriction)
  {
    Outpost::ParticleSystem particles(TICKS_PER_SECOND);
    particles.Add(Kind::ExplosionCore, ORIGIN, {10.0f, 0.0f, 0.0f}, 1.0f, 0.0f, 0.0, SEED);
    const auto x = [&particles](double _seconds)
    {
      const std::vector<Glow> glows = GlowsAt(particles, _seconds);
      Assert::AreEqual(size_t{1}, glows.size());
      return glows.front().position.x;
    };
    const float atHalf = x(0.5);
    const float atOne = x(1.0);
    Assert::AreEqual(10.0f * (1.0f - std::exp(-0.2f)) / 0.2f, atOne, TOLERANCE);
    Assert::IsTrue(atOne - atHalf < atHalf, L"the second half second covers less than the first");
  }

  // A particle keeps its brightness for three quarters of its life, then fades to nothing; a core lives 2 s.
  TEST_METHOD(AParticleFadesOverItsLastQuarter)
  {
    Outpost::ParticleSystem particles(TICKS_PER_SECOND);
    particles.Add(Kind::ExplosionCore, ORIGIN, {}, 1.0f, 0.0f, 0.0, SEED);
    const auto red = [&particles](double _seconds) { return GlowsAt(particles, _seconds).front().color.x; };
    const float full = red(0.5);
    Assert::IsTrue(full > 0.0f);
    Assert::AreEqual(full, red(1.4), TOLERANCE);
    Assert::AreEqual(full * 0.5f, red(1.75), TOLERANCE);
    Assert::IsTrue(GlowsAt(particles, 2.1).empty());
    Assert::AreEqual(size_t{0}, particles.Pending());
  }

  // Debris leaves puffs of smoke behind it as it flies, which outlive it by their own life, 2 s, and then it is forgotten.
  TEST_METHOD(DebrisLeavesATrailBehindIt)
  {
    Outpost::ParticleSystem particles(TICKS_PER_SECOND);
    particles.Add(Kind::ExplosionDebris, ORIGIN, {20.0f, 0.0f, 0.0f}, 1.0f, 0.0f, 0.0, SEED);
    const std::vector<Glow> flying = GlowsAt(particles, 3.0);
    Assert::IsTrue(flying.size() > 1, L"the debris and its trail");
    // The debris is the first glow, and its trail lies behind it.
    const float debrisX = flying.front().position.x;
    Assert::IsTrue(std::ranges::all_of(flying, [debrisX](const Glow& _glow) { return _glow.position.x <= debrisX + TOLERANCE; }));
    Assert::IsTrue(std::ranges::all_of(flying.begin() + 1, flying.end(),
                                       [&flying](const Glow& _glow) { return _glow.radiusMeters < flying.front().radiusMeters; }));

    Assert::IsTrue(GlowsAt(particles, 8.1).empty());
    Assert::AreEqual(size_t{0}, particles.Pending());
  }

  // A blast is sized by what blew up: a bigger footprint throws more, and farther.
  TEST_METHOD(ABiggerBlastReachesFarther)
  {
    Outpost::ParticleSystem smallBlast(TICKS_PER_SECOND);
    Outpost::ParticleSystem largeBlast(TICKS_PER_SECOND);
    smallBlast.AddBlast(ORIGIN, 8.0f, Outpost::EntityKind::Ship, 0.0, SEED);
    largeBlast.AddBlast(ORIGIN, 24.0f, Outpost::EntityKind::Ship, 0.0, SEED);
    Assert::IsTrue(largeBlast.Pending() > smallBlast.Pending());
    Assert::IsTrue(Reach(GlowsAt(largeBlast, 1.0)) > Reach(GlowsAt(smallBlast, 1.0)));
  }

  // As a DeepSpaceOutpost building does, a structure throws a flash of fast puffs as well.
  TEST_METHOD(AStructureThrowsMoreThanAShip)
  {
    Outpost::ParticleSystem ship(TICKS_PER_SECOND);
    Outpost::ParticleSystem structure(TICKS_PER_SECOND);
    ship.AddBlast(ORIGIN, 20.0f, Outpost::EntityKind::Ship, 0.0, SEED);
    structure.AddBlast(ORIGIN, 20.0f, Outpost::EntityKind::Structure, 0.0, SEED);
    Assert::IsTrue(structure.Pending() > ship.Pending());
  }

  TEST_METHOD(KeepsAtMostMaxParticles)
  {
    Outpost::ParticleSystem particles(TICKS_PER_SECOND);
    for (size_t i = 0; i < Outpost::ParticleSystem::MAX_PARTICLES + 10; ++i)
      particles.Add(Kind::ExplosionCore, ORIGIN, {}, 1.0f, 0.0f, 0.0, SEED);
    Assert::AreEqual(Outpost::ParticleSystem::MAX_PARTICLES, particles.Pending());
  }
};
} // namespace GameAppTests
