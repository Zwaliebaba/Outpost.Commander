#pragma once

namespace Outpost
{
// The minimum that makes combat readable (design §11, task 3.5): a muzzle flash where a shot leaves, a tracer or a beam
// to where it lands, a spark there, and a blast ring as wide as its splash for a splash weapon (task 5.3). A ship or
// structure destroyed is the ParticleSystem's and the ExplosionManager's (ADR-026). Hits are instant on the server
// (design §7); everything here is presentation, played from the shots the snapshots report, at the moment the view
// reaches them. It keeps no GPU state: it says which flat shapes to draw, and GameClient draws them.
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

  // Where a shooter's gun is in the view, given the point it fires at (ADR-018): GameClient finds it from the shooter's
  // model and where the view draws the ship. Nothing when it cannot, and the shot then leaves from where the server
  // says the ship stood when it fired.
  // It takes the shooter and the point it fires at.
  using MuzzleLocator = std::function<std::optional<PlanePosition>(EntityId, PlanePosition)>;

  explicit CombatEffects(std::uint32_t _ticksPerSecond);

  // Takes the shots of a snapshot. Each plays from one tick before the snapshot's own: the shot was fired from where the
  // ships stood at the start of that tick, and the view shows the snapshot's world a tick late (ADR-013).
  void Receive(const Snapshot& _snapshot);

  // What to draw with the view at _viewTick. Effects that have played out are forgotten. A shot leaves from the
  // shooter's muzzle as _muzzle finds it in this frame, so it follows a ship that moves while it fires.
  [[nodiscard]] std::vector<Draw> At(double _viewTick, const MuzzleLocator& _muzzle = {});

  // Effects waiting or playing; for tests.
  [[nodiscard]] size_t Pending() const noexcept
  {
    return m_effects.size();
  }

private:
  enum class Kind : std::uint8_t
  {
    Tracer,
    Beam
  };

  struct Effect
  {
    Kind kind = Kind::Tracer;
    double startTick = 0.0;
    EntityId shooter;
    PlanePosition from;
    PlanePosition to;
    // The shot's splash; zero for a shot without splash.
    float radiusMeters = 0.0f;
  };

  void AddShot(const Effect& _effect, double _tick, std::vector<Draw>& _draws) const;

  [[nodiscard]] double Seconds(double _ticks) const noexcept
  {
    return _ticks / m_ticksPerSecond;
  }

  double m_ticksPerSecond = 0.0;
  std::vector<Effect> m_effects;
};
} // namespace Outpost