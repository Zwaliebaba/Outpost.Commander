#include "pch.h"

#include "Replica.h"
#include "RenderViewBuilder.h"

#include "Host.h"
#include "Interest.h"

#include "Placement.h"

#include "LoopbackTransport.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The claim of TechnicalDesign.md §10, made against the real thing (m1-vertical-slice/R1):
// "applying a host's frames, with and without loss, produces a replica equal to the host's interest
// set at every acked frame".
//
// A REAL Sim, N2's Host AND N3's Client OVER THE LOOPBACK TRANSPORT. Nothing here is a stub or a
// recorded stream. The comparison is against Net's own encoders rather than against a second copy
// of what the replica did, so a bug shared by the encoder and the replica cannot cancel out: what
// is asserted is that the client holds what the HOST WOULD SEND IF IT SENT EVERYTHING NOW.
namespace ReplicaTests
{

namespace
{

constexpr std::int32_t CELL = Neuron::SUBUNITS_PER_CELL;
constexpr Neuron::LivenessSettings LIVENESS = {20, 100, 200};
constexpr std::uint64_t CONTENT = 0xFEEDFACEu;

/// How long the fixture's cannon draws a shot for. Ten ticks is half a second: long enough that a
/// render time in the middle of it is unambiguous, short enough that a case can step past the end.
constexpr std::uint32_t SHOT_LIFETIME_TICKS = 10;

const Outpost::ContentTree& Tables()
{
  static const Outpost::ContentTree TREE = []
  {
    Outpost::ContentTree tree;
    // MODELS, SO THAT THE RENDER-VIEW CASE HAS SOMETHING TO COMPOSE. The convergence cases do not
    // care - a model is content and is never replicated - but a row naming no model composes
    // nothing at all, and a render view that came back empty would pass a test that only counted.
    Outpost::ModelDesc hull{};
    hull.version = Outpost::MODEL_DESC_VERSION;
    hull.id = "LightHull";
    hull.vertices = {{-256, 0, -256}, {256, 0, -256}, {0, 512, 0}};
    // A MOUNT AS WELL AS A DRIVE (m1-vertical-slice/C8). Without one the fixture's device carried a
    // module it never drew and a weapon whose muzzle therefore did not exist, so the shot case
    // below would have had nowhere to fire from - and the instance counts further down were the
    // counts of a device that was missing a part.
    // THE MOUNT IS EIGHT WORLD UNITS UP, AND THAT NUMBER IS LOAD-BEARING. The shot case below
    // asserts that a projectile leaves the MUZZLE rather than the device's origin, and a mount one
    // world unit off the floor put the two within the tolerance that the interpolated position
    // needs - so a builder that started every shot at y = 0 passed. Measured: it does not now.
    hull.markers = {Outpost::ModelMarker{"MarkerDrive1", {0, 0, 0}, 0, 0}, Outpost::ModelMarker{"MarkerMount1", {0, 2048, 0}, 0, 0}};
    Outpost::ModelDesc wheel{};
    wheel.version = Outpost::MODEL_DESC_VERSION;
    wheel.id = "Wheel";
    wheel.vertices = {{-64, 0, -64}, {64, 0, 64}};
    Outpost::ModelDesc postHull{};
    postHull.version = Outpost::MODEL_DESC_VERSION;
    postHull.id = "PostHull";
    postHull.vertices = {{-512, 0, -512}, {512, 1024, 512}};
    Outpost::ModelDesc barrel{};
    barrel.version = Outpost::MODEL_DESC_VERSION;
    barrel.id = "CannonBarrel";
    barrel.vertices = {{-32, 0, -32}, {32, 64, 512}};
    barrel.markers = {Outpost::ModelMarker{"MarkerMuzzle", {0, 0, 512}, 0, 0}};
    Outpost::ModelDesc shell{};
    shell.version = Outpost::MODEL_DESC_VERSION;
    shell.id = "Shell";
    shell.vertices = {{-16, -16, -16}, {16, 16, 16}};
    tree.models = {hull, wheel, postHull, barrel, shell};

    Outpost::StructureDesc post{};
    post.id = "CommandPost";
    post.model = "PostHull";
    post.role = Outpost::StructureRole::CommandPost;
    post.footprintCellsX = 3;
    post.footprintCellsY = 3;
    post.hitPoints = 1500;
    post.costHundredths = 50000;
    post.buildTimeTicks = 40;
    tree.structures.structures = {post};

    Outpost::ChassisDesc light{};
    light.id = "LightI";
    light.chassisClass = Outpost::ChassisClass::Light;
    light.hitPoints = 100;
    light.sightSubunits = 6 * CELL;
    light.costHundredths = 6000;
    light.mounts = 1;
    // A SPEED, unlike the fixture NetTests uses. Nothing there ever walks, so a chassis at zero
    // subunits a tick was never noticed; the walking test below needs a device that can actually go.
    light.baseSpeedSubunitsPerTick = 1024;
    light.model = "LightHull";
    tree.components.chassis = {light};

    Outpost::DriveDesc wheels{};
    wheels.id = "Wheels";
    wheels.driveClass = Outpost::DriveClass::Wheels;
    wheels.speedFactorHundredths = 100;
    wheels.maxSlopePercent = 40;
    wheels.hitPointFactorHundredths = 100;
    wheels.model = "Wheel";
    wheels.costHundredths = 3000;
    tree.components.drives = {wheels};

    Outpost::ModuleDesc cannon{};
    cannon.id = "Cannon";
    cannon.systemKind = Outpost::SystemKind::None;
    cannon.costHundredths = 5000;
    cannon.model = "CannonBarrel";
    cannon.projectileModel = "Shell";
    cannon.projectileLifetimeTicks = SHOT_LIFETIME_TICKS;
    tree.components.modules = {cannon};
    return tree;
  }();
  return TREE;
}

Outpost::MatchSettings Lobby()
{
  Outpost::MatchSettings settings{};
  settings.seed = 17;
  settings.sizeClass = Outpost::SizeClass::Small;
  settings.seatCount = 2;
  settings.baseLevel = Outpost::BaseLevel::Nothing;
  settings.powerLevel = Outpost::PowerLevel::Medium;
  settings.deviceCapLevel = Outpost::DeviceCapLevel::Medium;
  settings.victory = Outpost::VictoryCondition::Annihilation;
  settings.rejoinGraceTicks = 400; // long: nothing here is testing the drop to AI
  settings.seats[0] = {Outpost::SeatKind::Human, 0, false};
  settings.seats[1] = {Outpost::SeatKind::Human, 1, false};
  return settings;
}

Outpost::LandscapeDefinition Ground()
{
  Outpost::LandscapeDefinition definition{};
  definition.version = Outpost::LANDSCAPE_DEFINITION_VERSION;
  definition.sizeClass = Outpost::SizeClass::Small;
  definition.cellsPerSide = Outpost::SIZE_CLASS_CELLS[0];
  definition.seed = 5;
  definition.palette = "Default";
  definition.tiles = {{0, 0, 512, 170, 90, 100, 48, 70, 1, 32, ""}};
  definition.starts = {{16, 16}, {100, 100}};
  return definition;
}

Outpost::ObjectId DeviceAt(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _cellX, std::uint32_t _cellY)
{
  Outpost::Seat& seat = _sim.SeatAt(_seat);
  if (seat.designs.empty())
  {
    Outpost::DeviceDesign design{};
    design.chassis = 0;
    design.drive = 0;
    design.modules[0] = 0;
    design.moduleCount = 1;
    seat.designs.push_back(design);
  }
  Outpost::Device device{};
  device.seat = _seat;
  device.design = 0;
  device.x = static_cast<std::int32_t>(_cellX) * CELL + CELL / 2;
  device.z = static_cast<std::int32_t>(_cellY) * CELL + CELL / 2;
  device.hitPoints = 100;
  device.target = Outpost::NO_OBJECT;
  return _sim.Objects().Create(device);
}

Outpost::ObjectId StructureAt(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _cellX, std::uint32_t _cellY)
{
  Outpost::Structure structure{};
  structure.seat = _seat;
  structure.design = 0;
  structure.cellX = _cellX;
  structure.cellY = _cellY;
  structure.state = Outpost::StructurePhase::Standing;
  structure.hitPoints = 1500;
  return _sim.Objects().Create(structure);
}

void Reveal(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _cellX, std::uint32_t _cellY, std::uint32_t _cells)
{
  for (std::uint32_t y = _cellY; y < _cellY + _cells; ++y)
  {
    for (std::uint32_t x = _cellX; x < _cellX + _cells; ++x)
    {
      _sim.SeatAt(_seat).fog.AddViewer(x, y);
    }
  }
}

void Conceal(Outpost::Sim& _sim, std::uint8_t _seat, std::uint32_t _cellX, std::uint32_t _cellY, std::uint32_t _cells)
{
  for (std::uint32_t y = _cellY; y < _cellY + _cells; ++y)
  {
    for (std::uint32_t x = _cellX; x < _cellX + _cells; ++x)
    {
      _sim.SeatAt(_seat).fog.RemoveViewer(x, y);
    }
  }
}

/// The whole loop in one object: the simulation, the host, the network, the client and the replica
/// the client fills.
struct Match
{
  explicit Match(std::uint64_t _faultSeed = 21)
    : sim(Lobby(), Tables()),
      network(_faultSeed),
      clientEnd(network.Connect()),
      host(sim, network.Host(), CONTENT, 0),
      client(clientEnd, LIVENESS, 0),
      replica(client)
  {
    Assert::IsTrue(sim.CreateLandscape(Ground()));
  }

  void Join(std::uint8_t _seat, std::uint64_t _token = 1)
  {
    client.SendJoin(CONTENT, _token, "owner", 0);
    Pump(6);
    Assert::IsTrue(client.State() == Outpost::ClientState::Playing, L"the join was accepted");
    Assert::AreEqual(_seat, client.Seat(), L"and it put this client in the seat the test expects");
  }

  void Pump(std::uint32_t _passes, bool _advanceSim = true)
  {
    for (std::uint32_t index = 0; index < _passes; ++index)
    {
      if (_advanceSim)
      {
        sim.Advance();
      }
      network.Host().Poll();
      host.Advance(sim.Tick());
      clientEnd.Poll();
      client.Advance(sim.Tick(), replica);
    }
  }

  /// Runs the match for a while and then keeps it running with nothing happening, so that the
  /// client catches up to a state that is no longer moving.
  ///
  /// THE SETTLING TICKS ARE ORDINARY TICKS. An earlier version of this stopped the simulation and
  /// pumped the network on its own, on the reasoning that the host would republish until it was
  /// acknowledged - it does - but that is a state the game is never in, and it took the sequence
  /// numbers twenty publishes past a tick that never changed. What convergence has to be measured
  /// against is the loop the game actually runs; nothing in this match gives an order, so once the
  /// opening frames are through, the state stands still while the ticks keep coming, and a client
  /// that is one publish behind a state that is not moving is a client that agrees with it.
  void RunAndSettle(std::uint32_t _ticks)
  {
    Pump(_ticks + 24);
  }

  Outpost::Sim sim;
  Neuron::LoopbackTransport network;
  Neuron::Transport& clientEnd;
  Outpost::Host host;
  Outpost::Client client;
  Outpost::Replica replica;
};

/// Levels the host's ground under a structure and writes the mean into it, which is what
/// GameLogic/Construction.cpp does the moment construction begins and what StructureAt above does not -
/// it builds a structure directly, so every one of them stands at y = 0 until this is called.
/// m1-vertical-slice/K6's case needs the host's landscape actually flattened, or there is nothing
/// for the client's to agree with.
void LevelOnHost(Outpost::Sim& _sim, Outpost::ObjectId _id)
{
  Outpost::Structure* structure = _sim.Objects().FindStructure(_id);
  Assert::IsNotNull(structure, L"the structure to level under");
  const Outpost::Footprint footprint = Outpost::FootprintOf(*structure, &Tables());
  const Outpost::HeightDelta delta = Outpost::FlattenDelta(_sim.Terrain(), footprint);
  Assert::IsFalse(delta.heights.empty(), L"the footprint is on the landscape");
  const std::int32_t mean = Outpost::FootprintMeanHeightWorldUnits(_sim.Terrain(), footprint);
  Assert::IsTrue(_sim.FlattenTerrain(delta), L"the host levelled its own ground");
  structure = _sim.Objects().FindStructure(_id);
  Assert::IsNotNull(structure);
  structure->y = mean * Neuron::SUBUNITS_PER_WORLD_UNIT;
}

[[nodiscard]] std::size_t CountProjectiles(const Neuron::RenderView& _view)
{
  std::size_t count = 0;
  for (const Neuron::RenderInstance& instance : _view.instances)
  {
    count += instance.kind == Neuron::RenderInstanceKind::Projectile ? 1 : 0;
  }
  return count;
}

[[nodiscard]] const Neuron::RenderInstance* FindProjectile(const Neuron::RenderView& _view)
{
  for (const Neuron::RenderInstance& instance : _view.instances)
  {
    if (instance.kind == Neuron::RenderInstanceKind::Projectile)
    {
      return &instance;
    }
  }
  return nullptr;
}

/// The assertion this whole file is for: the replica holds exactly what the host would send this
/// seat if it sent everything now, object for object and field for field.
void AssertConverged(const Outpost::Sim& _sim, std::uint8_t _seat, const Outpost::Replica& _replica, const wchar_t* _where)
{
  Outpost::InterestSet interest;
  Outpost::GatherInterest(_sim, _seat, interest);
  const Outpost::World& world = _sim.Objects();
  const Outpost::Replica& replica = _replica;

  Assert::AreEqual(interest.devices.size(), replica.Devices().size(), _where);
  for (const std::uint32_t id : interest.devices)
  {
    const Outpost::Device* device = world.FindDevice({id, Outpost::ObjectKind::Device});
    Assert::IsNotNull(device, _where);
    const auto held = replica.Devices().find(id);
    Assert::IsTrue(held != replica.Devices().end(), _where);
    Assert::IsTrue(held->second.state == Outpost::WireDevice(id, *device), _where);
  }

  Assert::AreEqual(interest.structures.size() + interest.ghosts.size(), replica.Structures().size(), _where);
  for (const std::uint32_t id : interest.structures)
  {
    const Outpost::Structure* structure = world.FindStructure({id, Outpost::ObjectKind::Structure});
    Assert::IsNotNull(structure, _where);
    const auto held = replica.Structures().find(id);
    Assert::IsTrue(held != replica.Structures().end(), _where);
    Assert::IsTrue(held->second.state == Outpost::WireStructure(_sim.Content(), id, *structure), _where);
    Assert::IsFalse(held->second.ghost, L"a structure in sight is not a ghost");
  }
  for (const Outpost::ObjectId& id : interest.ghosts)
  {
    const Outpost::Ghost* ghost = _sim.Seats()[_seat].ghosts.Find(id);
    Assert::IsNotNull(ghost, _where);
    const auto held = replica.Structures().find(id.value);
    Assert::IsTrue(held != replica.Structures().end(), _where);
    Assert::IsTrue(held->second.state == Outpost::WireGhost(*ghost), _where);
    Assert::IsTrue(held->second.ghost, L"a structure out of sight is a ghost");
  }

  Assert::AreEqual(interest.wrecks.size(), replica.Wrecks().size(), _where);
  for (const std::uint32_t id : interest.wrecks)
  {
    const Outpost::Wreck* wreck = world.FindWreck({id, Outpost::ObjectKind::Wreck});
    Assert::IsNotNull(wreck, _where);
    const auto held = replica.Wrecks().find(id);
    Assert::IsTrue(held != replica.Wrecks().end(), _where);
    Assert::IsTrue(held->second.state == Outpost::WireWreck(id, *wreck), _where);
  }

  Assert::AreEqual(interest.features.size(), replica.Features().size(), _where);
  for (const std::uint32_t id : interest.features)
  {
    const Outpost::Feature* feature = world.FindFeature({id, Outpost::ObjectKind::Feature});
    Assert::IsNotNull(feature, _where);
    const auto held = replica.Features().find(id);
    Assert::IsTrue(held != replica.Features().end(), _where);
    Assert::IsTrue(held->second.state == Outpost::WireFeature(id, *feature), _where);
  }

  // And the commander's own state, which is the one record that is his rather than an object's.
  // The latch is empty because nothing in this file sends an order, so nothing is refused; if that
  // ever stops being true this line fails rather than quietly comparing less than it says it does.
  const Outpost::RejectionLatch nothingRefused;
  Assert::IsTrue(replica.Own() == Outpost::WireSeat(_seat, _sim.Seats()[_seat], nothingRefused), _where);

  // AND THE FOG, CELL FOR CELL. This suite compared only its SIZE until m1-vertical-slice/G1a,
  // which is a comparison an all-black grid passes - and an all-black grid is exactly what the
  // first capture of a real match produced, for a bug that had been in the encoder since N2. The
  // two spans are compared flat because flat is what the wire carries: GameLogic/FrameEncoder.cpp reads
  // seat.fog.States() and Replica::ApplyFog writes the same indices, so an index computed here
  // would be a third opinion about the layout rather than a check of it.
  const std::span<const Outpost::FogState> hostFog = _sim.Seats()[_seat].fog.States();
  Assert::AreEqual(hostFog.size(), replica.Fog().size(), _where);
  std::size_t disagreeing = 0;
  std::size_t firstDisagreement = 0;
  for (std::size_t cell = 0; cell < hostFog.size(); ++cell)
  {
    if (hostFog[cell] != replica.Fog()[cell])
    {
      firstDisagreement = disagreeing == 0 ? cell : firstDisagreement;
      ++disagreeing;
    }
  }
  // COUNTED, AND THE FIRST ONE NAMED. A grid that has diverged diverges in hundreds of cells, so
  // asserting per cell buries every other finding in the run; and "a cell disagrees" says nothing
  // about which way, which is the difference between a fog that never arrived and one that arrived
  // and was not withdrawn. Written only on failure, so it costs a passing run nothing.
  if (disagreeing != 0)
  {
    Logger::WriteMessage((L"    fog: " + std::to_wstring(disagreeing) + L" of " + std::to_wstring(hostFog.size()) +
                          L" cells disagree; the first is cell " + std::to_wstring(firstDisagreement) + L", host " +
                          std::to_wstring(static_cast<int>(hostFog[firstDisagreement])) + L", replica " +
                          std::to_wstring(static_cast<int>(replica.Fog()[firstDisagreement])) + L"\n")
                           .c_str());
  }
  Assert::AreEqual(std::size_t{0}, disagreeing, _where);
}

} // namespace

TEST_CLASS(ConvergenceTests)
{
public:
  /// THE BUG THIS CASE IS NAMED FOR: the host republishes FULL frames until a baseline is
  /// acknowledged, and a replica clears everything it holds - fog included - on each one
  /// (Replica::Apply). The encoder re-sent the whole fog only on the FIRST full frame, so the
  /// second blacked out a map the first had drawn correctly and nothing ever refilled it. The
  /// commander's map stayed black for the rest of the match.
  ///
  /// Found in m1-vertical-slice/G1a, not here: the first capture of a real match came back 100%
  /// black on a green build, because the only fog this suite compared was its SIZE.
  TEST_METHOD(ASecondFullFrameDoesNotBlackOutAMapTheFirstOneDrew)
  {
    Match match;
    StructureAt(match.sim, 0, 14, 14);
    Reveal(match.sim, 0, 10, 10, 20);
    match.Join(0);

    // Pass by pass rather than in one go, so that a failure names the publish that lost the map
    // rather than reporting that it is gone.
    for (std::uint32_t pass = 1; pass <= 12; ++pass)
    {
      match.Pump(1);
      const std::span<const Outpost::FogState> hostFog = match.sim.Seats()[0].fog.States();
      Assert::AreEqual(hostFog.size(), match.replica.Fog().size(), L"the grid kept its size");
      std::size_t visible = 0;
      std::size_t agreeing = 0;
      for (std::size_t cell = 0; cell < hostFog.size(); ++cell)
      {
        visible += hostFog[cell] == Outpost::FogState::Visible ? 1 : 0;
        agreeing += hostFog[cell] == match.replica.Fog()[cell] ? 1 : 0;
      }
      Assert::IsTrue(visible > 0, L"the host has cells this commander can see, or the case tests nothing");
      Logger::WriteMessage((L"pass " + std::to_wstring(pass) + L": " + std::to_wstring(agreeing) + L" of " +
                            std::to_wstring(hostFog.size()) + L" cells agree\n")
                             .c_str());
      Assert::AreEqual(hostFog.size(), agreeing, L"every cell of the commander's map survived this publish");
    }

    // AND A SETTLED MATCH SENDS NO FOG AT ALL, which is the half of the fix the assertion above
    // cannot see. Correctness would survive a host that re-sent the whole grid on every publish -
    // the replica would agree with it every time - and that is exactly what a host does if it never
    // folds an acknowledged baseline into the grid it encodes against. Mutation testing found this
    // gap: removing the fold left all nine cases passing. Nothing is moving by here, so a frame
    // that carries a fog run is a frame telling this commander something he already knew.
    match.Pump(8);
    Assert::IsTrue(match.replica.ChangedFogRows().empty(), L"a settled match publishes no fog runs");
  }

  TEST_METHOD(TheReplicaEqualsTheHostsInterestSetOverAWholeMatch)
  {
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    DeviceAt(match.sim, 0, 18, 16);
    DeviceAt(match.sim, 1, 24, 24);
    StructureAt(match.sim, 0, 14, 14);
    StructureAt(match.sim, 1, 26, 26);
    Reveal(match.sim, 0, 10, 10, 20);
    match.Join(0);

    for (std::uint32_t round = 0; round < 8; ++round)
    {
      match.RunAndSettle(6);
      AssertConverged(match.sim, 0, match.replica, L"round after round, with no loss");
    }
    Assert::AreEqual(std::uint32_t{0}, match.network.Statistics().dropped, L"this run lost nothing");
    Assert::IsTrue(match.replica.NewestSequence() != Outpost::NO_BASELINE, L"frames were applied at all");
    // AGAINST A VACUOUS PASS. Every assertion above compares the replica with the interest set, and
    // two empty things are equal: without this line a replica that applied nothing at all would
    // sail through the whole test.
    Assert::IsTrue(match.replica.ObjectCount() >= 5, L"and there was something to compare");
  }

  /// The same claim with a quarter of the datagrams thrown away, which is what makes it worth
  /// making: a replica that converged only on a clean network would be a replica that cannot be
  /// used on one that is not.
  TEST_METHOD(TheReplicaConvergesThroughSeededLoss)
  {
    Match match(90210);
    match.network.SetFaults(Neuron::LoopbackFaults{250, 0, 0, 0, 0});
    DeviceAt(match.sim, 0, 16, 16);
    DeviceAt(match.sim, 0, 18, 16);
    DeviceAt(match.sim, 1, 24, 24);
    StructureAt(match.sim, 0, 14, 14);
    Reveal(match.sim, 0, 10, 10, 20);
    match.Join(0);

    for (std::uint32_t round = 0; round < 8; ++round)
    {
      match.RunAndSettle(6);
      AssertConverged(match.sim, 0, match.replica, L"round after round, through loss");
    }
    Assert::IsTrue(match.network.Statistics().dropped > 0, L"the network actually lost something");
    Assert::IsTrue(match.replica.ObjectCount() >= 4, L"and the replica holds what it converged on");
  }

  /// A ghost is the last thing a commander saw, kept after the building leaves his sight and
  /// corrected when he sees it again (GameDesign.md §5). Nothing in the replica decides this: the
  /// host sends the remembered record in the same list, and the replica's job is to not lose it.
  TEST_METHOD(AGhostPersistsOutOfSightAndIsCorrectedWhenSeenAgain)
  {
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    const Outpost::ObjectId watched = StructureAt(match.sim, 1, 30, 30);
    Reveal(match.sim, 0, 10, 10, 30);
    match.Join(0);

    match.RunAndSettle(6);
    AssertConverged(match.sim, 0, match.replica, L"seen");
    const auto seen = match.replica.Structures().find(watched.value);
    Assert::IsTrue(seen != match.replica.Structures().end(), L"the enemy structure is held");
    Assert::IsFalse(seen->second.ghost, L"and it is not a ghost while it is in sight");
    const std::uint16_t hitPointsWhenSeen = seen->second.state.hitPoints;
    Assert::IsTrue(hitPointsWhenSeen > 0);

    // The commander looks away, and the building takes damage he cannot see.
    Conceal(match.sim, 0, 10, 10, 30);
    Outpost::Structure* structure = match.sim.Objects().FindStructure(watched);
    Assert::IsNotNull(structure);
    structure->hitPoints = 400;
    match.RunAndSettle(6);
    AssertConverged(match.sim, 0, match.replica, L"out of sight");

    const auto remembered = match.replica.Structures().find(watched.value);
    Assert::IsTrue(remembered != match.replica.Structures().end(), L"the ghost is still there");
    Assert::IsTrue(remembered->second.ghost, L"and it is a ghost now");
    Assert::AreEqual(Outpost::GHOST_HIT_POINTS, remembered->second.state.hitPoints,
                     L"a ghost carries no hit points: he has no idea what it has taken");

    // A GHOST IS DRAWN AND NOT PICKED, which is the rule RenderViewBuilder carries and which only
    // shows where there IS a ghost - the render-view case has none, so with the check removed there
    // it passed. Interface.md §5's inspection is for what is visible; a click on a remembered
    // building would select something that may no longer be there.
    const Outpost::ModelComposer composer(Tables());
    Outpost::RenderViewSettings settings{};
    settings.cellsPerSide = Ground().cellsPerSide;
    Outpost::RenderViewBuilder builder(Tables(), composer, settings);
    Neuron::RenderView view;
    Outpost::PickSet picks;
    builder.Build(match.replica, match.replica.RenderTimeAtNewestFrame(), std::span<const std::uint32_t>{}, view, picks);
    Assert::IsFalse(view.instances.empty(), L"the ghost is still drawn");
    for (const Outpost::PickCandidate& candidate : picks.candidates)
    {
      Assert::IsTrue(candidate.id != watched.value, L"but nothing can click it");
    }

    // He looks back, and the damage he never saw is there when he arrives.
    Reveal(match.sim, 0, 10, 10, 30);
    match.RunAndSettle(6);
    AssertConverged(match.sim, 0, match.replica, L"seen again");

    const auto again = match.replica.Structures().find(watched.value);
    Assert::IsTrue(again != match.replica.Structures().end());
    Assert::IsFalse(again->second.ghost, L"in sight again, so not a ghost");
    Assert::AreEqual(std::uint16_t{400}, again->second.state.hitPoints, L"and the damage is there");
  }

  /// The second of the three things a frame carries, and the one every other test here leaves
  /// alone: a CHANGE. Nothing else in this file gives an order, so nothing else moves, so a replica
  /// that threw every position delta away would pass all of them. This one walks a device across
  /// the landscape and holds the replica to it.
  ///
  /// WHAT IS ASSERTED WHILE IT WALKS IS NOT FULL CONVERGENCE, and that is not a weakening. A
  /// replica is one publish behind a world that is moving - that is what interpolating 100
  /// milliseconds back is FOR (TechnicalDesign.md §3) - so a replica that equalled the simulation
  /// mid-stride would mean the client had guessed ahead, which is the one thing Interpolation.h
  /// refuses to do. The claim while it moves is that the replica holds the same objects and its
  /// copy of the device follows; the claim once it stops is the full one, and it is made below.
  TEST_METHOD(ADeviceThatWalksIsFollowedDeltaByDelta)
  {
    Match match;
    const Outpost::ObjectId walker = DeviceAt(match.sim, 0, 16, 16);
    StructureAt(match.sim, 0, 14, 14);
    Reveal(match.sim, 0, 8, 8, 30);
    match.Join(0);
    match.RunAndSettle(4);
    AssertConverged(match.sim, 0, match.replica, L"before it is told to go");

    const Outpost::Device* device = match.sim.Objects().FindDevice(walker);
    Assert::IsNotNull(device);
    const std::int32_t startX = device->x;
    const auto atStart = match.replica.Devices().find(walker.value);
    Assert::IsTrue(atStart != match.replica.Devices().end());
    const std::int32_t replicaStartX = atStart->second.state.x;

    Outpost::Order walk{};
    walk.tick = match.sim.Tick() + 1;
    walk.seat = 0;
    walk.kind = Outpost::OrderKind::Move;
    walk.operands = {static_cast<std::int32_t>(walker.value), 26 * CELL + CELL / 2, 16 * CELL + CELL / 2, 0};
    match.sim.Submit(walk);

    for (std::uint32_t round = 0; round < 10; ++round)
    {
      match.Pump(8);
      Outpost::InterestSet interest;
      Outpost::GatherInterest(match.sim, 0, interest);
      Assert::AreEqual(interest.devices.size(), match.replica.Devices().size(), L"the same devices all the way");
      Assert::AreEqual(interest.structures.size() + interest.ghosts.size(), match.replica.Structures().size(), L"and structures");
    }

    const Outpost::Device* arrived = match.sim.Objects().FindDevice(walker);
    Assert::IsNotNull(arrived);
    Assert::IsTrue(arrived->x != startX, L"the order took and the device actually walked");

    const auto walked = match.replica.Devices().find(walker.value);
    Assert::IsTrue(walked != match.replica.Devices().end());
    // THE ASSERTION THAT A REPLICA IGNORING POSITION DELTAS FAILS. Every other test in this file
    // passes with the deltas thrown away, because nothing else in them ever moves.
    Assert::IsTrue(walked->second.state.x != replicaStartX, L"and the replica followed it");
    Assert::IsTrue(walked->second.motion.HasSegment(), L"with two samples to draw between");

    const Outpost::Pose pose = Outpost::Evaluate(walked->second.motion, match.replica.RenderTimeAtNewestFrame());
    const float olderX = static_cast<float>(walked->second.motion.older.x) / 4.0f;
    const float newerX = static_cast<float>(walked->second.motion.newer.x) / 4.0f;
    Assert::IsTrue(pose.x >= std::min(olderX, newerX) - 0.001f && pose.x <= std::max(olderX, newerX) + 0.001f,
                   L"and what it draws is on the segment between them");

    // And now the full claim, once the world stops moving and the client has caught up with it.
    match.Pump(200);
    AssertConverged(match.sim, 0, match.replica, L"after the walk, standing still");
  }

  /// The third of the three things a frame carries, and the one a test is most likely to leave
  /// uncovered: a removal. A creation is obvious when it is missing and a change shows up as a
  /// position that drifts, but an object that is never removed just sits there, and every other
  /// assertion in this file would still pass around it.
  TEST_METHOD(AnObjectRemovedWhileTheCommanderIsWatchingLeavesTheReplica)
  {
    Match match;
    const Outpost::ObjectId doomed = DeviceAt(match.sim, 0, 16, 16);
    const Outpost::ObjectId spared = DeviceAt(match.sim, 0, 18, 16);
    const Outpost::ObjectId razed = StructureAt(match.sim, 0, 14, 14);
    // THE SECOND STRUCTURE IS NOT SCENERY. Razing a commander's last one annihilates him, S11 ends
    // the match, and Sim::Advance stops on m_finished - so the frames this test is waiting for
    // never come and it fails somewhere else entirely. The first draft of this test did exactly
    // that, and the simulation was right both times.
    StructureAt(match.sim, 0, 20, 20);
    Reveal(match.sim, 0, 10, 10, 20);
    match.Join(0);
    match.RunAndSettle(6);
    AssertConverged(match.sim, 0, match.replica, L"before anything is removed");
    Assert::IsTrue(match.replica.Devices().find(doomed.value) != match.replica.Devices().end());
    Assert::IsTrue(match.replica.Structures().find(razed.value) != match.replica.Structures().end());

    // No rejoin and no full frame: the client stays connected, so the only way either of these can
    // leave the replica is through the removed list of an ordinary delta.
    Assert::IsTrue(match.sim.Objects().Remove(doomed));
    Assert::IsTrue(match.sim.Objects().Remove(razed));
    match.RunAndSettle(6);

    Assert::IsTrue(match.replica.Devices().find(doomed.value) == match.replica.Devices().end(),
                   L"the device went with the frame that said so");
    Assert::IsTrue(match.replica.Structures().find(razed.value) == match.replica.Structures().end(), L"and so did the structure");
    Assert::IsTrue(match.replica.Devices().find(spared.value) != match.replica.Devices().end(), L"and nothing else went with them");
    AssertConverged(match.sim, 0, match.replica, L"after the removals");
  }

  /// A rejoin is a full frame, and a full frame is everything: whatever the replica held before it
  /// was encoded against a baseline the new frame is not a delta from, so keeping any of it would
  /// leave objects the host has since removed standing on the field forever.
  TEST_METHOD(ARejoinRebuildsTheReplicaFromAFullFrame)
  {
    Match match;
    const Outpost::ObjectId doomed = DeviceAt(match.sim, 0, 16, 16);
    DeviceAt(match.sim, 0, 18, 16);
    StructureAt(match.sim, 0, 14, 14);
    Reveal(match.sim, 0, 10, 10, 20);
    match.Join(0);
    match.RunAndSettle(6);
    AssertConverged(match.sim, 0, match.replica, L"before the rejoin");
    const std::size_t before = match.replica.ObjectCount();
    Assert::IsTrue(before >= 3, L"there is something to lose");

    // The device goes while the client is not listening, so a replica that kept what it held would
    // still be drawing it after the rejoin.
    Assert::IsTrue(match.sim.Objects().Remove(doomed));
    match.Pump(6);

    Outpost::Client rejoined(match.clientEnd, LIVENESS, match.sim.Tick());
    Outpost::Replica rebuilt(rejoined);
    rejoined.SendJoin(CONTENT, 1, "owner", match.sim.Tick());
    for (std::uint32_t index = 0; index < 40; ++index)
    {
      match.network.Host().Poll();
      match.host.Advance(match.sim.Tick());
      match.clientEnd.Poll();
      rejoined.Advance(match.sim.Tick(), rebuilt);
    }

    Assert::IsTrue(rejoined.State() == Outpost::ClientState::Playing, L"the rejoin was accepted");
    Assert::IsTrue(rebuilt.Devices().find(doomed.value) == rebuilt.Devices().end(), L"what was destroyed while away is not still held");
    AssertConverged(match.sim, 0, rebuilt, L"after the rejoin");
  }

  /// The replica is one publish interval behind by design, and its own timeline says so. This is
  /// the number the executable draws at (TechnicalDesign.md §3 step 4).
  /// THE RENDER VIEW BUILT FROM A CONVERGED REPLICA, which is the one part of
  /// GameClient/RenderViewBuilder.h that needs the whole stack: Build takes a Replica, a Replica needs
  /// a Net::Client, and a Client needs a host to have sent it a JoinAccepted. The arithmetic the
  /// builder does on its own - the chunks a footprint touches - is a free function with its own
  /// cases in RenderViewTests; what is here is the walk, which can only be exercised against a
  /// replica that a real host filled.
  TEST_METHOD(TheRenderViewHoldsWhatTheConvergedReplicaDoes)
  {
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    DeviceAt(match.sim, 0, 18, 16);
    const Outpost::ObjectId post = StructureAt(match.sim, 0, 14, 14);
    // A SECOND STRUCTURE, for the reason the removal case gives: razing a commander's last one
    // annihilates him, S11 ends the match, and the frames this case waits for never come.
    StructureAt(match.sim, 0, 20, 20);
    Reveal(match.sim, 0, 10, 10, 20);
    match.Join(0);
    match.RunAndSettle(6);
    AssertConverged(match.sim, 0, match.replica, L"before anything is drawn from it");

    const Outpost::ModelComposer composer(Tables());
    Outpost::RenderViewSettings settings{};
    settings.cellsPerSide = Ground().cellsPerSide;
    settings.chunkCells = 32;
    Outpost::RenderViewBuilder builder(Tables(), composer, settings);

    Neuron::RenderView view;
    Outpost::PickSet picks;
    const std::uint32_t selected = match.replica.Devices().begin()->first;
    const std::array<std::uint32_t, 1> selection{selected};
    builder.Build(match.replica, match.replica.RenderTimeAtNewestFrame(), selection, view, picks);

    Assert::AreEqual(std::uint32_t{0}, builder.LastUnresolvedObjects(), L"every object resolved to content");
    Assert::AreEqual(std::size_t{2}, match.replica.Devices().size(), L"the two devices arrived");
    // Two devices, each a hull, a wheel at its one MarkerDrive and a cannon at its one MarkerMount,
    // and two command posts: eight. It was six until C8 gave the fixture's hull a MarkerMount - the
    // device had always carried the module and never drawn it.
    Assert::AreEqual(std::size_t{8}, view.instances.size(), L"a hull, a drive and a module each, and two posts");
    Assert::AreEqual(std::size_t{4}, picks.candidates.size(), L"two devices and two structures");

    std::size_t selectedInstances = 0;
    for (const Neuron::RenderInstance& instance : view.instances)
    {
      Assert::AreEqual(std::uint8_t{0}, instance.colorIndex, L"everything visible here is seat 0's");
      selectedInstances += instance.selected ? 1 : 0;
    }
    Assert::AreEqual(std::size_t{3}, selectedInstances, L"EVERY part of the one selected device, and no other");

    // The fog reaches the view whole, which is what the fog pass and the minimap both read.
    Assert::AreEqual(match.replica.FogCellsPerSide(), view.fog.cellsPerSide);
    Assert::AreEqual(match.replica.Fog().size(), view.fog.cells.size());
    Assert::IsTrue(view.fog.cellsPerSide > 0, L"and there is a grid at all");

    // THE CHUNKS ARE REPORTED ONCE. The post arrived this frame, so its chunk is in the list; a
    // second build with nothing new reports none, which is the whole reason the builder remembers
    // what it flattened rather than re-flattening a base on every frame of the match.
    Assert::IsFalse(view.changedChunks.empty(), L"the post flattened its ground");
    Neuron::RenderView again;
    Outpost::PickSet againPicks;
    builder.Build(match.replica, match.replica.RenderTimeAtNewestFrame(), selection, again, againPicks);
    Assert::IsTrue(again.changedChunks.empty(), L"and nothing changed the second time");
    Assert::AreEqual(view.instances.size(), again.instances.size(), L"though everything is still drawn");

    // A RAZED BUILDING LEAVES ITS GROUND AS IT LEFT IT, so its chunks change when it goes just as
    // they did when it arrived. Nothing else in this file removes a structure while a render view
    // is being built, and with this branch taken out the case above still passed.
    Assert::IsTrue(match.sim.Objects().Remove(post));
    match.RunAndSettle(6);
    Neuron::RenderView afterRazing;
    Outpost::PickSet afterRazingPicks;
    builder.Build(match.replica, match.replica.RenderTimeAtNewestFrame(), selection, afterRazing, afterRazingPicks);
    Assert::IsFalse(afterRazing.changedChunks.empty(), L"the ground under it has to be rebuilt");
    Assert::AreEqual(std::size_t{7}, afterRazing.instances.size(), L"and the post is no longer drawn");

    // And forgetting brings them all back, which is what a rejoin's full frame needs.
    builder.ForgetTerrain();
    Neuron::RenderView afterRejoin;
    Outpost::PickSet afterPicks;
    builder.Build(match.replica, match.replica.RenderTimeAtNewestFrame(), selection, afterRejoin, afterPicks);
    Assert::IsFalse(afterRejoin.changedChunks.empty(), L"the terrain is rebuilt from nothing");
  }

  TEST_METHOD(TheGroundUnderWhatHeCanSeeFollowsTheHostAndTheRestOfItDoesNot)
  {
    // m1-vertical-slice/K6, end to end and the whole point of it. The wire carries no height
    // deltas - they would hand a commander the shape of ground he has never scouted
    // (TechnicalDesign.md §5.2) - so the client levels its OWN landscape under the structures it
    // can see, from the render view's flattens. What has to be true is both halves at once: under
    // a structure he can see, his samples are the host's, sample for sample; under a base he has
    // never scouted, they are still the ones the generator made.
    Match match;
    const Outpost::ObjectId mine = StructureAt(match.sim, 0, 14, 14);
    // SEAT 1's, and that is what makes it invisible. A commander always holds his own structures
    // whatever the fog says, so an unseen one of his own would prove nothing.
    const Outpost::ObjectId theirs = StructureAt(match.sim, 1, 90, 90);
    LevelOnHost(match.sim, mine);
    LevelOnHost(match.sim, theirs);
    Reveal(match.sim, 0, 10, 10, 20); // Round his own and nowhere near theirs
    match.Join(0);
    match.RunAndSettle(6);
    AssertConverged(match.sim, 0, match.replica, L"before the ground is levelled");
    Assert::IsTrue(match.replica.Structures().contains(mine.value), L"he holds his own");
    Assert::IsFalse(match.replica.Structures().contains(theirs.value), L"and not the one across the map");

    const Outpost::ModelComposer composer(Tables());
    Outpost::RenderViewSettings settings{};
    settings.cellsPerSide = Ground().cellsPerSide;
    settings.chunkCells = 32;
    Outpost::RenderViewBuilder builder(Tables(), composer, settings);
    Neuron::RenderView view;
    Outpost::PickSet picks;
    builder.Build(match.replica, match.replica.RenderTimeAtNewestFrame(), {}, view, picks);
    Assert::AreEqual(std::size_t{1}, view.flattens.size(), L"one structure to level under, and it is his");
    Assert::IsFalse(view.changedChunks.empty(), L"and its chunk is named for the mesh");

    // The client's own landscape, generated from the definition the join carried and levelled from
    // the view - which is exactly what OutpostCommander/Match.cpp does with the same call.
    Outpost::Landscape client;
    Assert::IsTrue(client.Create(Ground()));
    Assert::AreEqual(std::uint32_t{1}, Outpost::LevelUnderFlattens(client, view.flattens), L"the one flatten was applied");

    // A third landscape, generated and untouched, so that "still the generator's" is a comparison
    // and not an assumption.
    Outpost::Landscape pristine;
    Assert::IsTrue(pristine.Create(Ground()));

    const Outpost::Structure* hisPost = match.sim.Objects().FindStructure(mine);
    Assert::IsNotNull(hisPost);
    const Outpost::Footprint his = Outpost::FootprintOf(*hisPost, &Tables());
    const Outpost::HeightDelta hisWindow = Outpost::FlattenDelta(pristine, his);
    std::size_t agreed = 0;
    std::size_t differedFromPristine = 0;
    for (std::uint32_t y = hisWindow.y; y < hisWindow.y + hisWindow.height; ++y)
    {
      for (std::uint32_t x = hisWindow.x; x < hisWindow.x + hisWindow.width; ++x)
      {
        Assert::AreEqual(match.sim.Terrain().HeightAt(x, y), client.HeightAt(x, y), L"his ground is the host's, sample for sample");
        ++agreed;
        differedFromPristine += client.HeightAt(x, y) != pristine.HeightAt(x, y) ? 1 : 0;
      }
    }
    Assert::IsTrue(agreed > 0, L"there were samples to compare");
    Assert::IsTrue(differedFromPristine > 0, L"and levelling them actually changed something");

    const Outpost::Structure* theirPost = match.sim.Objects().FindStructure(theirs);
    Assert::IsNotNull(theirPost);
    const Outpost::Footprint hers = Outpost::FootprintOf(*theirPost, &Tables());
    const Outpost::HeightDelta herWindow = Outpost::FlattenDelta(pristine, hers);
    std::size_t hostMoved = 0;
    for (std::uint32_t y = herWindow.y; y < herWindow.y + herWindow.height; ++y)
    {
      for (std::uint32_t x = herWindow.x; x < herWindow.x + herWindow.width; ++x)
      {
        Assert::AreEqual(pristine.HeightAt(x, y), client.HeightAt(x, y), L"the terrain under an unscouted base is not public");
        hostMoved += match.sim.Terrain().HeightAt(x, y) != pristine.HeightAt(x, y) ? 1 : 0;
      }
    }
    // Without this the case would pass on a host that had not levelled anything there either.
    Assert::IsTrue(hostMoved > 0, L"the host really had flattened that ground");
  }

  TEST_METHOD(TheLevelledHeightIsTheHostsAndNotOneTheClientWorksOutForItself)
  {
    // THE CASE THAT SEPARATES THE TWO WAYS OF DOING THIS (m1-vertical-slice/K6). FlattenDelta fills
    // its rectangle with the mean of the landscape it is GIVEN, so a client that kept that value
    // would be right whenever its ground matched the host's when the host levelled - which is most
    // of the time, and is why this needs setting up deliberately.
    //
    // Two structures whose flatten WINDOWS overlap: a window is the samples a footprint's cells own
    // plus the boundary sample on the far side, so two footprints side by side share a column of
    // samples. The host levels the first and then the second, so the second's mean is taken over
    // ground the first already flattened. The commander can see only the SECOND. His own landscape
    // therefore has nothing under the first, and a mean he worked out himself would be taken over
    // different samples than the host's was. The wire carried the host's answer, and that is what
    // has to land.
    Match match;
    const Outpost::ObjectId hidden = StructureAt(match.sim, 1, 14, 14);
    const Outpost::ObjectId shown = StructureAt(match.sim, 0, 17, 14); // Cells 17..19, touching 14..16

    // A hill under the hidden one, so that levelling it MOVES the shared column by enough to shift
    // the other's mean. The fixture's own ground is nearly flat there, and on flat ground the two
    // answers agree - which is exactly the reason this defect could ship unnoticed.
    Outpost::HeightDelta hill{};
    hill.x = 14 * Outpost::SAMPLES_PER_CELL_EDGE;
    hill.y = 14 * Outpost::SAMPLES_PER_CELL_EDGE;
    hill.width = 3 * Outpost::SAMPLES_PER_CELL_EDGE + 1;
    hill.height = 3 * Outpost::SAMPLES_PER_CELL_EDGE + 1;
    hill.heights.assign(static_cast<std::size_t>(hill.width) * hill.height, 400);
    Assert::IsTrue(match.sim.FlattenTerrain(hill), L"the hill went in");

    LevelOnHost(match.sim, hidden);
    LevelOnHost(match.sim, shown);   // Second, so its mean is over ground the first already levelled
    Reveal(match.sim, 0, 17, 13, 5); // Cells 17..21, clear of the hidden one at 14..16
    match.Join(0);
    match.RunAndSettle(6);
    Assert::IsTrue(match.replica.Structures().contains(shown.value), L"he holds the one beside him");
    Assert::IsFalse(match.replica.Structures().contains(hidden.value), L"and not the one behind it");

    const Outpost::ModelComposer composer(Tables());
    Outpost::RenderViewSettings settings{};
    settings.cellsPerSide = Ground().cellsPerSide;
    settings.chunkCells = 32;
    Outpost::RenderViewBuilder builder(Tables(), composer, settings);
    Neuron::RenderView view;
    Outpost::PickSet picks;
    builder.Build(match.replica, match.replica.RenderTimeAtNewestFrame(), {}, view, picks);
    Assert::AreEqual(std::size_t{1}, view.flattens.size(), L"one flatten, the one he can see");

    Outpost::Landscape client;
    Assert::IsTrue(client.Create(Ground()));
    Assert::AreEqual(std::uint32_t{1}, Outpost::LevelUnderFlattens(client, view.flattens));

    // What the client would have worked out for itself, on ITS ground: the number this case exists
    // to be different from. If it is not different the fixture has stopped testing anything, so
    // that is asserted rather than assumed.
    Outpost::Landscape pristine;
    Assert::IsTrue(pristine.Create(Ground()));
    const Outpost::Structure* structure = match.sim.Objects().FindStructure(shown);
    Assert::IsNotNull(structure);
    const Outpost::Footprint footprint = Outpost::FootprintOf(*structure, &Tables());
    const std::int32_t hostHeight = structure->y / Neuron::SUBUNITS_PER_WORLD_UNIT;
    const std::int32_t local = Outpost::FootprintMeanHeightWorldUnits(pristine, footprint);
    Logger::WriteMessage(("    measured: the host levelled to " + std::to_string(hostHeight) +
                          " and the client's own ground would have said " + std::to_string(local) + "\n")
                           .c_str());
    Assert::AreNotEqual(hostHeight, local, L"the two answers differ here, or this case proves nothing");

    const Outpost::HeightDelta window = Outpost::FlattenDelta(pristine, footprint);
    for (std::uint32_t y = window.y; y < window.y + window.height; ++y)
    {
      for (std::uint32_t x = window.x; x < window.x + window.width; ++x)
      {
        Assert::AreEqual(static_cast<std::int16_t>(hostHeight), client.HeightAt(x, y), L"the host's height and not the client's own");
        Assert::AreEqual(match.sim.Terrain().HeightAt(x, y), client.HeightAt(x, y), L"which is the host's ground, sample for sample");
      }
    }
  }

  TEST_METHOD(AShotIsDrawnFromTheMuzzleTowardWhatItWasAimedAtAndThenIsGone)
  {
    // m1-vertical-slice/C8, the whole chain: an event the host records, the wire, the replica, and
    // an instance of RenderInstanceKind::Projectile where the shot has got to. It is here rather
    // than in RenderViewTests because a Shot has to ARRIVE to be drawn - the builder reads
    // Replica::Events(), which only a frame fills - and this file is the one that stands the stack
    // up.
    Match match;
    const Outpost::ObjectId shooter = DeviceAt(match.sim, 0, 16, 16);
    StructureAt(match.sim, 0, 20, 20); // So that razing nothing ends no match
    Reveal(match.sim, 0, 10, 10, 20);
    match.Join(0);
    match.RunAndSettle(6);
    AssertConverged(match.sim, 0, match.replica, L"before anything is fired");

    const Outpost::ModelComposer composer(Tables());
    Outpost::RenderViewSettings settings{};
    settings.cellsPerSide = Ground().cellsPerSide;
    settings.chunkCells = 32;
    Outpost::RenderViewBuilder builder(Tables(), composer, settings);
    Neuron::RenderView view;
    Outpost::PickSet picks;
    builder.Build(match.replica, match.replica.RenderTimeAtNewestFrame(), {}, view, picks);
    Assert::AreEqual(std::size_t{0}, CountProjectiles(view), L"nothing has fired yet");

    // THE EVENT IS MADE BY HAND, AND THAT IS THE RIGHT SEAM HERE. m1-vertical-slice/C9 gave the
    // channel its producer - Sim records every trigger pull and LocalHost::RecordShots turns it
    // into this - and CombatTests and HostTests are where that half is measured. What is under
    // test here is the OTHER half: a Shot that arrives becomes something drawn. Standing up a real
    // firefight to get one would make this case fail for a dozen reasons that are not its own.
    // The shot is aimed four cells east of the shooter.
    const std::uint32_t firedTick = match.sim.Tick();
    const std::int32_t aimedX = (16 + 4) * CELL + CELL / 2;
    const std::int32_t aimedZ = 16 * CELL + CELL / 2;
    Outpost::Event shot{};
    shot.kind = Outpost::EventKind::Shot;
    shot.source = shooter.value;
    shot.sourceKind = Outpost::ObjectKind::Device;
    shot.target = 0;
    shot.targetKind = Outpost::ObjectKind::Device;
    shot.x = Outpost::WireFromSubunits(aimedX);
    shot.y = 0;
    shot.z = Outpost::WireFromSubunits(aimedZ);
    shot.tick = firedTick;
    match.host.Events().push_back(shot);

    // ONE PASS AT A TIME, BUILDING AFTER EACH, WHICH IS WHAT THE CLIENT LOOP DOES. An event is a
    // thing that HAPPENED: Replica::Events() holds the newest frame's and is replaced by every
    // Apply, so a test that pumped four passes and then looked would find the shot gone - the frame
    // that carried it had been overwritten by two that carried nothing. OutpostCommander/Match.cpp
    // builds the render view immediately after applying, and so does this.
    bool arrived = false;
    for (std::uint32_t pass = 0; pass < 8 && !arrived; ++pass)
    {
      match.Pump(1);
      for (const Outpost::Event& event : match.replica.Events())
      {
        arrived = arrived || (event.kind == Outpost::EventKind::Shot && event.source == shooter.value);
      }
      builder.Build(match.replica, match.replica.RenderTimeAtNewestFrame(), {}, view, picks);
    }
    Assert::IsTrue(arrived, L"the event crossed the wire to the commander who could see the shooter");

    // Halfway through the shot's life, it is halfway between the muzzle and what it was aimed at.
    // The builder REMEMBERS it, so this build needs no event of its own - which is the thing that
    // makes a shot outlive the one frame that announced it.
    const std::int64_t halfway = Outpost::RenderTimeOfTick(firedTick) + Outpost::RenderTimeOfTick(SHOT_LIFETIME_TICKS) / 2;
    builder.Build(match.replica, halfway, {}, view, picks);
    Assert::AreEqual(std::size_t{1}, CountProjectiles(view), L"one shot in the air");
    const Neuron::RenderInstance* drawn = FindProjectile(view);
    Assert::IsNotNull(drawn);

    // Where the muzzle is: the hull's MarkerMount1 then the barrel's MarkerMuzzle, which the
    // composer works out and this case does not duplicate - it asks for the same composition.
    std::vector<Neuron::RenderInstance> parts;
    std::vector<Outpost::ComposedMuzzle> muzzles;
    const Outpost::DesignState* design = match.replica.Designs().Find(0, 0);
    Assert::IsNotNull(design);
    const auto held = match.replica.Devices().find(shooter.value);
    Assert::IsTrue(held != match.replica.Devices().end());
    const Outpost::Pose pose = Outpost::Evaluate(held->second.motion, halfway);
    composer.ComposeDevice(*design, pose, {}, parts, &muzzles);
    Assert::AreEqual(std::size_t{1}, muzzles.size(), L"the cannon has a muzzle");

    const float aimedWorldX = static_cast<float>(aimedX) / static_cast<float>(Neuron::SUBUNITS_PER_WORLD_UNIT);
    const float aimedWorldZ = static_cast<float>(aimedZ) / static_cast<float>(Neuron::SUBUNITS_PER_WORLD_UNIT);
    Assert::AreEqual((muzzles.front().x + aimedWorldX) * 0.5f, drawn->x, 1.0f, L"halfway along x");
    Assert::AreEqual((muzzles.front().z + aimedWorldZ) * 0.5f, drawn->z, 1.0f, L"halfway along z");
    // IT STARTS AT THE MUZZLE AND NOT AT THE DEVICE'S ORIGIN, which is the difference the
    // ComposedMuzzle exists for: a shot leaving the barrel rather than the floor.
    Assert::AreNotEqual(pose.y, muzzles.front().y, L"the fixture's muzzle is off the ground, or this proves nothing");
    Assert::AreEqual((muzzles.front().y + 0.0f) * 0.5f, drawn->y, 0.1f, L"and halfway down to what it was aimed at");

    // At the tick it was fired it is AT the muzzle, and one tick past its lifetime it is gone.
    builder.Build(match.replica, Outpost::RenderTimeOfTick(firedTick), {}, view, picks);
    const Neuron::RenderInstance* atTheMuzzle = FindProjectile(view);
    Assert::IsNotNull(atTheMuzzle);
    Assert::AreEqual(muzzles.front().x, atTheMuzzle->x, 1.0f, L"it leaves from the muzzle");

    // AND IT IS NOT DRAWN BEFORE IT WAS FIRED, which is the other end of the same interval. A
    // frame covers a publish interval and a client draws several frames inside one, so a render
    // time earlier than the trigger is ordinary rather than exotic - and without the guard the
    // shot is drawn a negative way along its own flight, behind the barrel.
    builder.Build(match.replica, Outpost::RenderTimeOfTick(firedTick) - Outpost::RENDER_TIME_SCALE, {}, view, picks);
    Assert::AreEqual(std::size_t{0}, CountProjectiles(view), L"a tick before the trigger there is nothing in the air");

    builder.Build(match.replica, Outpost::RenderTimeOfTick(firedTick + SHOT_LIFETIME_TICKS), {}, view, picks);
    Assert::AreEqual(std::size_t{0}, CountProjectiles(view), L"its lifetime is over and it is not drawn");
  }

  TEST_METHOD(TheReplicasRenderTimeIsOnePublishIntervalBehindItsNewestFrame)
  {
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 10, 10, 20);
    match.Join(0);
    match.RunAndSettle(10);

    Assert::IsTrue(match.replica.NewestTick() > 0);
    const std::int64_t expected =
      Outpost::RenderTimeOfTick(match.replica.NewestTick()) - Outpost::INTERPOLATION_DELAY_TICKS * Outpost::RENDER_TIME_SCALE;
    Assert::AreEqual(expected, match.replica.RenderTimeAtNewestFrame());
  }
};

} // namespace ReplicaTests
