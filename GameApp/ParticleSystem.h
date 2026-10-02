#pragma once

namespace Outpost
{
// Particles after DeepSpaceOutpost's ParticleSystem (ADR-023): diamonds of light that fly out, slow under friction and
// fade, drawn as glows (ADR-019) with DeepSpaceOutpost's particle texture. A particle's place is a closed form of its age, so what is drawn depends only on the
// view's tick, as task 3.5's effects do, never on the frame rate. Space has no ground and no down, so DeepSpaceOutpost's
// gravity and bouncing are left out. It is presentation: the server knows nothing of it.
class ParticleSystem
{
public:
  enum class Kind : std::uint8_t
  {
    // A puff of the fireball: big, short-lived, red.
    ExplosionCore,
    // A burning fragment: small, long-lived, and leaving a trail of smoke as it flies.
    ExplosionDebris
  };

  // Particles kept at once, waiting or playing. Past it, a new one is dropped.
  static constexpr size_t MAX_PARTICLES = 4096;

  explicit ParticleSystem(std::uint32_t _ticksPerSecond);

  // One particle of _kind, from _position at _velocity, from _birthTick. _colorShare, from 0 to 1, picks its color
  // between its kind's two, and _seed decides where a debris particle leaves its trail.
  void Add(Kind _kind, const DirectX::XMFLOAT3& _position, const DirectX::XMFLOAT3& _velocity, float _radiusMeters, float _colorShare,
           double _birthTick, std::uint64_t _seed);

  // The blast of a ship or a structure destroyed at _center, with the footprint _radiusMeters, from _startTick:
  // DeepSpaceOutpost's Location::Bang, a fireball and debris, and for a structure also the fast flash of its
  // Building::Destroy. _seed decides every particle, so the same seed gives the same blast.
  void AddBlast(const DirectX::XMFLOAT3& _center, float _radiusMeters, EntityKind _kind, double _startTick, std::uint64_t _seed);

  // Appends the glows of every particle at _viewTick to _glows. Particles that have played out are forgotten.
  void At(double _viewTick, std::vector<Neuron::GlowPipeline::Glow>& _glows);

  // Particles waiting or playing; for tests. A trail is drawn from its debris and is not counted.
  [[nodiscard]] size_t Pending() const noexcept
  {
    return m_particles.size();
  }

private:
  struct Particle
  {
    Kind kind = Kind::ExplosionCore;
    double birthTick = 0.0;
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 velocity{};
    float radiusMeters = 0.0f;
    DirectX::XMFLOAT4 color{};
    std::uint64_t seed = 0;
  };

  void AddTrail(const Particle& _debris, double _ageSeconds, std::vector<Neuron::GlowPipeline::Glow>& _glows) const;

  [[nodiscard]] double Seconds(double _ticks) const noexcept
  {
    return _ticks / m_ticksPerSecond;
  }

  double m_ticksPerSecond = 0.0;
  std::vector<Particle> m_particles;
};
} // namespace Outpost
