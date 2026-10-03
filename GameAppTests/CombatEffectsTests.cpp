#include "pch.h"

#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr std::uint32_t TICKS_PER_SECOND = 20;
constexpr Outpost::WeaponId MASS_DRIVER{1};
constexpr Outpost::WeaponId LANCE{2};
constexpr Outpost::WeaponId RAIL_CANNON{5};
constexpr Outpost::PlanePosition GUN{.xMeters = 0.0f, .zMeters = 0.0f};
constexpr Outpost::PlanePosition TARGET{.xMeters = 100.0f, .zMeters = 0.0f};
// Where a test's view finds the shooter's gun.
constexpr Outpost::PlanePosition MUZZLE{.xMeters = 6.0f, .zMeters = 2.0f};

// A snapshot at _tick reporting one shot.
Outpost::Snapshot Shot(std::uint64_t _tick, Outpost::WeaponId _weapon, float _splashRadiusMeters = 0.0f)
{
  Outpost::Snapshot snapshot{.tick = _tick, .player = Outpost::PlayerId{1}};
  snapshot.shots.push_back({.shooter = Outpost::EntityId{1},
                            .target = Outpost::EntityId{2},
                            .weapon = _weapon,
                            .from = GUN,
                            .to = TARGET,
                            .splashRadiusMeters = _splashRadiusMeters});
  return snapshot;
}

// Ticks after the shot's start, which is one tick before its snapshot.
double After(std::uint64_t _snapshotTick, double _seconds)
{
  return static_cast<double>(_snapshotTick) - 1.0 + (_seconds * TICKS_PER_SECOND);
}

// The effects with the repository's looks: the Lance's beam and the Rail Cannon's slug (ADR-034).
Outpost::CombatEffects Effects()
{
  return Outpost::CombatEffects(
    TICKS_PER_SECOND, {{.weapon = LANCE, .look = Outpost::ShotLook::Beam}, {.weapon = RAIL_CANNON, .look = Outpost::ShotLook::Slug}});
}

size_t CountOf(const std::vector<Outpost::CombatEffects::Draw>& _draws, Outpost::CombatEffects::Shape _shape)
{
  return static_cast<size_t>(std::ranges::count(_draws, _shape, &Outpost::CombatEffects::Draw::shape));
}
} // namespace

TEST_CLASS(CombatEffectsTests)
{
public:
  // Task 3.5: an effect plays when the view reaches the tick its shot was fired at, one before its snapshot.
  TEST_METHOD(WaitsForTheViewToReachTheShot)
  {
    Outpost::CombatEffects effects = Effects();
    effects.Receive(Shot(10, MASS_DRIVER));
    Assert::IsTrue(effects.At(8.5).empty());
    const std::vector<Outpost::CombatEffects::Draw> draws = effects.At(9.0);
    Assert::AreEqual(size_t{1}, CountOf(draws, Outpost::CombatEffects::Shape::Disc), L"a muzzle flash");
    Assert::AreEqual(size_t{1}, CountOf(draws, Outpost::CombatEffects::Shape::Band), L"a tracer");
  }

  // A tracer's head crosses from the gun to the target, and a spark shows where it lands.
  TEST_METHOD(ATracerCrossesThenSparks)
  {
    Outpost::CombatEffects effects = Effects();
    effects.Receive(Shot(10, MASS_DRIVER));
    const std::vector<Outpost::CombatEffects::Draw> midway = effects.At(After(10, 0.06));
    const auto band = std::ranges::find(midway, Outpost::CombatEffects::Shape::Band, &Outpost::CombatEffects::Draw::shape);
    Assert::IsTrue(band != midway.end());
    Assert::AreEqual(50.0f, band->to.xMeters, 0.01f);
    Assert::IsTrue(band->from.xMeters > 0.0f && band->from.xMeters < band->to.xMeters);

    const std::vector<Outpost::CombatEffects::Draw> landed = effects.At(After(10, 0.15));
    Assert::AreEqual(size_t{0}, CountOf(landed, Outpost::CombatEffects::Shape::Band));
    Assert::AreEqual(size_t{1}, landed.size());
    Assert::IsTrue(landed.front().from == TARGET, L"the spark is at the target");
  }

  // Task 5.3: a splash weapon's hit throws a ring at the target that runs out to its splash radius, then fades.
  TEST_METHOD(ASplashRingRunsOutToItsRadius)
  {
    Outpost::CombatEffects effects = Effects();
    effects.Receive(Shot(10, Outpost::WeaponId{3}, 30.0f));
    const auto ring = [&effects](double _seconds)
    {
      const std::vector<Outpost::CombatEffects::Draw> draws = effects.At(After(10, _seconds));
      const auto found = std::ranges::find(draws, Outpost::CombatEffects::Shape::Ring, &Outpost::CombatEffects::Draw::shape);
      return found != draws.end() && found->from == TARGET ? found->radiusMeters : -1.0f;
    };
    Assert::AreEqual(-1.0f, ring(0.06), L"no ring while the tracer flies");
    const float early = ring(0.15);
    const float late = ring(0.38);
    Assert::IsTrue(early > 0.0f && late > early && late <= 30.0f);
    Assert::AreEqual(-1.0f, ring(0.45), L"gone once it has run out");

    effects.Receive(Shot(40, MASS_DRIVER));
    Assert::AreEqual(size_t{0}, CountOf(effects.At(After(40, 0.15)), Outpost::CombatEffects::Shape::Ring), L"no splash, no ring");
  }

  // A Lance's beam joins the gun and the target at once.
  TEST_METHOD(ABeamJoinsGunAndTarget)
  {
    Outpost::CombatEffects effects = Effects();
    effects.Receive(Shot(10, LANCE));
    const std::vector<Outpost::CombatEffects::Draw> draws = effects.At(After(10, 0.01));
    const auto band = std::ranges::find(draws, Outpost::CombatEffects::Shape::Band, &Outpost::CombatEffects::Draw::shape);
    Assert::IsTrue(band != draws.end());
    Assert::IsTrue(band->from == GUN && band->to == TARGET);
  }

  // ADR-034: a Rail Cannon's slug joins the gun and the target at once, white whoever fires it, flashes at both ends, and
  // fades where it stands after a beam has gone.
  TEST_METHOD(ASlugJoinsGunAndTargetAndLingers)
  {
    Outpost::CombatEffects effects = Effects();
    effects.Receive(Shot(10, RAIL_CANNON));
    const auto slug = [&effects](double _seconds)
    {
      const std::vector<Outpost::CombatEffects::Draw> draws =
        effects.At(After(10, _seconds), {}, [](Outpost::EntityId) { return std::optional(DirectX::XMFLOAT4{1.0f, 0.0f, 0.0f, 1.0f}); });
      const auto band = std::ranges::find(draws, Outpost::CombatEffects::Shape::Band, &Outpost::CombatEffects::Draw::shape);
      return band != draws.end() ? std::optional(*band) : std::nullopt;
    };
    const std::optional<Outpost::CombatEffects::Draw> fresh = slug(0.01);
    Assert::IsTrue(fresh.has_value() && fresh->from == GUN && fresh->to == TARGET);
    Assert::IsTrue(fresh->color.y > 0.5f && fresh->color.z > 0.5f, L"white, not its side's color");
    const std::vector<Outpost::CombatEffects::Draw> flashes = effects.At(After(10, 0.01));
    Assert::IsTrue(std::ranges::any_of(flashes, [](const Outpost::CombatEffects::Draw& _draw)
                                       { return _draw.shape == Outpost::CombatEffects::Shape::Disc && _draw.from == TARGET; }),
                   L"a flash where it lands");
    const std::optional<Outpost::CombatEffects::Draw> late = slug(0.3);
    Assert::IsTrue(late.has_value() && late->widthMeters == fresh->widthMeters && late->color.x < fresh->color.x);
    Assert::IsFalse(slug(0.45).has_value());
  }

  // A weapon the looks do not list fires tracers, as the Flak Battery does, and so does the Lance when no looks are given.
  TEST_METHOD(AWeaponNotListedFiresTracers)
  {
    // Halfway through a tracer's flight its streak has left the gun behind; a beam still starts there.
    const auto bandsFromTheGun = [](Outpost::CombatEffects _effects)
    {
      _effects.Receive(Shot(10, Outpost::WeaponId{4}, 20.0f));
      _effects.Receive(Shot(10, LANCE));
      const std::vector<Outpost::CombatEffects::Draw> draws = _effects.At(After(10, 0.06));
      Assert::AreEqual(size_t{2}, CountOf(draws, Outpost::CombatEffects::Shape::Band));
      return static_cast<size_t>(
        std::ranges::count_if(draws, [](const Outpost::CombatEffects::Draw& _draw)
                              { return _draw.shape == Outpost::CombatEffects::Shape::Band && _draw.from == GUN; }));
    };
    Assert::AreEqual(size_t{1}, bandsFromTheGun(Effects()), L"the Lance's beam");
    Assert::AreEqual(size_t{0}, bandsFromTheGun(Outpost::CombatEffects(TICKS_PER_SECOND)), L"two tracers");
  }

  // ADR-018: a shot leaves from the shooter's muzzle where the view draws it this frame, or, when the view cannot find
  // it, from where the server says the ship stood.
  TEST_METHOD(AShotLeavesFromTheMuzzleTheViewFinds)
  {
    Outpost::CombatEffects effects = Effects();
    effects.Receive(Shot(10, LANCE));
    const auto beamFrom = [&effects](const Outpost::CombatEffects::MuzzleLocator& _muzzle)
    {
      const std::vector<Outpost::CombatEffects::Draw> draws = effects.At(After(10, 0.01), _muzzle);
      const auto band = std::ranges::find(draws, Outpost::CombatEffects::Shape::Band, &Outpost::CombatEffects::Draw::shape);
      Assert::IsTrue(band != draws.end() && band->to == TARGET);
      return band->from;
    };
    Assert::IsTrue(beamFrom([](Outpost::EntityId _shooter, Outpost::PlanePosition) -> std::optional<Outpost::PlanePosition>
                            { return _shooter == Outpost::EntityId{1} ? std::optional(MUZZLE) : std::nullopt; }) == MUZZLE);
    Assert::IsTrue(beamFrom([](Outpost::EntityId, Outpost::PlanePosition) { return std::optional<Outpost::PlanePosition>(); }) == GUN);
  }

  // ADR-028: a beam is in the color the view gives its shooter, so the player sees whose fire it is, and a neutral one
  // for a shooter the view cannot place.
  TEST_METHOD(ABeamTakesItsShootersColor)
  {
    Outpost::CombatEffects effects = Effects();
    effects.Receive(Shot(10, LANCE));
    static constexpr DirectX::XMFLOAT4 SIDE{1.0f, 0.5f, 0.25f, 1.0f};
    const auto beamColor = [&effects](const Outpost::CombatEffects::BeamTint& _tint)
    {
      const std::vector<Outpost::CombatEffects::Draw> draws = effects.At(After(10, 0.0), {}, _tint);
      const auto band = std::ranges::find(draws, Outpost::CombatEffects::Shape::Band, &Outpost::CombatEffects::Draw::shape);
      Assert::IsTrue(band != draws.end());
      return band->color;
    };
    const DirectX::XMFLOAT4 tinted = beamColor([](Outpost::EntityId _shooter) -> std::optional<DirectX::XMFLOAT4>
                                               { return _shooter == Outpost::EntityId{1} ? std::optional(SIDE) : std::nullopt; });
    Assert::AreEqual(SIDE.x, tinted.x);
    Assert::AreEqual(SIDE.y, tinted.y);
    Assert::AreEqual(SIDE.z, tinted.z);
    const DirectX::XMFLOAT4 neutral = beamColor([](Outpost::EntityId) { return std::optional<DirectX::XMFLOAT4>(); });
    Assert::AreNotEqual(SIDE.y, neutral.y);
    const DirectX::XMFLOAT4 untinted = beamColor({});
    Assert::AreEqual(neutral.y, untinted.y);
  }

  // ADR-026: what is destroyed is the particles' and the explosions', not the combat effects'.
  TEST_METHOD(LeavesWhatIsDestroyedToTheExplosions)
  {
    Outpost::CombatEffects effects = Effects();
    Outpost::Snapshot snapshot{.tick = 30, .player = Outpost::PlayerId{1}};
    snapshot.destroyed.push_back({.id = Outpost::EntityId{4}, .position = TARGET, .radiusMeters = 10.0f});
    effects.Receive(snapshot);
    Assert::AreEqual(size_t{0}, effects.Pending());
    Assert::IsTrue(effects.At(After(30, 0.1)).empty());
  }
};
} // namespace GameAppTests