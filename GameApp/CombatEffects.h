#pragma once

namespace Outpost
{
// The minimum that makes combat readable (design §11, task 3.5): a muzzle flash where a shot leaves, a tracer or a beam
// to where it lands, a spark there, a blast ring as wide as its splash for a splash weapon (task 5.3), and an explosion
// where a ship or structure is destroyed. Hits are instant on the
// server (design §7); everything here is presentation, played from the shots and destructions the snapshots report, at
// the moment the view reaches them. It keeps no GPU state: it says which flat shapes to draw, and GameClient draws them.
class CombatEffects
{
public:
  // A flat shape on a level plane above the ground, which is all an effect is made of.
  enum class Shape : std::uint8_t
  {
    // A filled circle of radiusMeters.
    Disc,
    // A thin circle of radiusMeters.
    Ring,
    // A band widthMeters wide from `from` to `to`.
    Band
  };

  struct Draw
  {
    Shape shape = Shape::Disc;
    PlanePosition from;
    PlanePosition to;
    float radiusMeters = 0.0f;
    float widthMeters = 0.0f;
    // How far above the ground, so that effects show over the ships they belong to.
    float heightMeters = 0.0f;
    // Linear color. The pipeline is opaque, so an effect fades by darkening toward black rather than by transparency.
    DirectX::XMFLOAT4 color{};
  };

  explicit CombatEffects(std::uint32_t _ticksPerSecond);

  // Takes the shots and destructions of a snapshot. Each plays from one tick before the snapshot's own: the shot was
  // fired from where the ships stood at the start of that tick, and the view shows the snapshot's world a tick late
  // (ADR-013).
  void Receive(const Snapshot& _snapshot);

  // What to draw with the view at _viewTick. Effects that have played out are forgotten.
  [[nodiscard]] std::vector<Draw> At(double _viewTick);

  // Effects waiting or playing; for tests.
  [[nodiscard]] size_t Pending() const noexcept
  {
    return m_effects.size();
  }

private:
  enum class Kind : std::uint8_t
  {
    Tracer,
    Beam,
    Explosion
  };

  struct Effect
  {
    Kind kind = Kind::Tracer;
    double startTick = 0.0;
    PlanePosition from;
    PlanePosition to;
    // An explosion's size, or a shot's splash; zero for a shot without splash.
    float radiusMeters = 0.0f;
  };

  void AddShot(const Effect& _effect, double _tick, std::vector<Draw>& _draws) const;
  void AddExplosion(const Effect& _effect, double _tick, std::vector<Draw>& _draws) const;

  [[nodiscard]] double Seconds(double _ticks) const noexcept
  {
    return _ticks / m_ticksPerSecond;
  }

  double m_ticksPerSecond = 0.0;
  std::vector<Effect> m_effects;
};
} // namespace Outpost