#include "pch.h"

#include "Client.h"
#include "FrameEncoder.h"
#include "Host.h"

#include "LoopbackTransport.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The conversation (TechnicalDesign.md §5.3 and §5.4; m1-vertical-slice/N2): a join answered with a
// full frame, deltas against what the client acknowledged, a lost frame answered by the next
// delta, a full frame when the history has run out, a frame too large for a datagram split and put
// back together, and a client that stops answering handed to the AI.
namespace NetTests
{

namespace
{

constexpr std::int32_t CELL = Neuron::SUBUNITS_PER_CELL;
constexpr Neuron::LivenessSettings LIVENESS = {20, 100, 200};
constexpr std::uint64_t CONTENT = 0xFEEDFACEu;

const Outpost::ContentTree& Tables()
{
  static const Outpost::ContentTree TREE = []
  {
    Outpost::ContentTree tree;
    Outpost::StructureDesc post{};
    post.id = "CommandPost";
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
    tree.components.chassis = {light};
    Outpost::DriveDesc wheels{};
    wheels.id = "Wheels";
    wheels.driveClass = Outpost::DriveClass::Wheels;
    wheels.speedFactorHundredths = 100;
    wheels.hitPointFactorHundredths = 100;
    wheels.costHundredths = 3000;
    tree.components.drives = {wheels};
    Outpost::ModuleDesc cannon{};
    cannon.id = "Cannon";
    cannon.systemKind = Outpost::SystemKind::None;
    cannon.costHundredths = 5000;
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
  settings.rejoinGraceTicks = 40; // short, so that the drop-to-AI case runs in ticks not minutes
  settings.seats[0] = {Outpost::SeatKind::Human, 0, false};
  settings.seats[1] = {Outpost::SeatKind::Human, 1, false};
  return settings;
}

/// A lobby with one human seat and one scripted one, for the observer cases: a client may watch a
/// seat that is AI and may not watch one a human holds, so both have to exist in the same match.
Outpost::MatchSettings LobbyWithAnAiSeat()
{
  Outpost::MatchSettings settings = Lobby();
  settings.seats[1] = {Outpost::SeatKind::Ai, 1, false};
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

class Recorder : public Outpost::FrameSink
{
public:
  void Apply(const Outpost::Frame& _frame) override
  {
    frames.push_back(_frame);
    for (const Outpost::DeviceState& device : _frame.createdDevices)
    {
      held[device.id] = device;
    }
    for (const Outpost::DeviceChange& change : _frame.changedDevices)
    {
      auto at = held.find(change.id);
      if (at == held.end())
      {
        continue;
      }
      if ((change.mask & static_cast<std::uint8_t>(Outpost::DeviceField::Position)) != 0)
      {
        at->second.x += change.deltaX;
        at->second.y += change.deltaY;
        at->second.z += change.deltaZ;
      }
      if ((change.mask & static_cast<std::uint8_t>(Outpost::DeviceField::HitPoints)) != 0)
      {
        at->second.hitPoints = change.hitPoints;
      }
    }
    for (const std::uint32_t id : _frame.removed)
    {
      held.erase(id);
    }
  }

  std::vector<Outpost::Frame> frames;
  std::map<std::uint32_t, Outpost::DeviceState> held;
};

/// One pass of the whole loop: the simulation, the host, the transport and the client.
struct Match
{
  explicit Match(const Outpost::MatchSettings& _settings = Lobby())
    : sim(_settings, Tables()),
      network(21),
      clientEnd(network.Connect()),
      host(sim, network.Host(), CONTENT, 0),
      client(clientEnd, LIVENESS, 0)
  {
    Assert::IsTrue(sim.CreateLandscape(Ground()));
  }

  void Pump(std::uint32_t _ticks, bool _advanceSim = true)
  {
    for (std::uint32_t index = 0; index < _ticks; ++index)
    {
      if (_advanceSim)
      {
        sim.Advance();
      }
      network.Host().Poll();
      host.Advance(sim.Tick());
      clientEnd.Poll();
      client.Advance(sim.Tick(), sink);
    }
  }

  Outpost::Sim sim;
  Neuron::LoopbackTransport network;
  Neuron::Transport& clientEnd;
  Outpost::Host host;
  Outpost::Client client;
  Recorder sink;
};

} // namespace

TEST_CLASS(HostTests)
{
public:
  TEST_METHOD(AJoinIsAnsweredAndTheFirstFrameIsAFullOne)
  {
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(8);

    Assert::IsTrue(match.client.State() == Outpost::ClientState::Playing);
    Assert::AreEqual(static_cast<int>(0), static_cast<int>(match.client.Seat()));
    Assert::AreEqual(static_cast<std::uint64_t>(5), match.client.Landscape().seed, L"the ground came with the seat");
    Assert::IsFalse(match.sink.frames.empty());
    Assert::AreEqual(Outpost::NO_BASELINE, match.sink.frames[0].baselineSequence, L"the first frame has no baseline");
    Assert::AreEqual(static_cast<std::size_t>(1), match.sink.frames[0].createdDevices.size());
    Assert::AreEqual(static_cast<std::uint32_t>(1), match.host.Statistics().joins);
  }

  TEST_METHOD(AJoinWithTheWrongContentOrProtocolIsRefused)
  {
    Match match;
    match.client.SendJoin(CONTENT + 1, 1, "stranger", 0);
    match.Pump(6);
    Assert::IsTrue(match.client.State() == Outpost::ClientState::Refused);
    Assert::IsTrue(match.client.Refusal() == Outpost::RefusalReason::ContentHash);
    Assert::AreEqual(static_cast<std::uint32_t>(1), match.host.Statistics().refusals);
  }

  TEST_METHOD(AFrameAfterTheFirstIsADeltaAgainstWhatTheClientAcknowledged)
  {
    Match match;
    const Outpost::ObjectId walker = DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(10);
    const std::size_t afterJoin = match.sink.frames.size();
    Assert::IsTrue(afterJoin >= 2);

    // Move the device a little and publish again: what goes out is a change and not a creation.
    match.sim.Objects().FindDevice(walker)->x += 4 * Outpost::SUBUNITS_PER_WIRE_UNIT;
    match.Pump(4);
    const Outpost::Frame& latest = match.sink.frames.back();
    Assert::AreNotEqual(Outpost::NO_BASELINE, latest.baselineSequence, L"a delta against the acked baseline");
    Assert::IsTrue(latest.createdDevices.empty(), L"nothing was created");
    Assert::IsTrue(match.host.Statistics().fullFrames <= 2, L"only the frames sent before the first acknowledgement came back were full");

    // The client's picture followed the simulation, which is what the delta is for.
    Assert::AreEqual(static_cast<std::size_t>(1), match.sink.held.size());
    Assert::AreEqual(Outpost::WireFromSubunits(match.sim.Objects().FindDevice(walker)->x), match.sink.held[walker.value].x);
  }

  TEST_METHOD(ADeviceThatDidNotMoveCostsNothing)
  {
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(16);
    const Outpost::Frame& quiet = match.sink.frames.back();
    Assert::IsTrue(quiet.createdDevices.empty());
    Assert::IsTrue(quiet.changedDevices.empty(), L"a device that did not move sends nothing (§5.3)");
    Assert::IsTrue(quiet.removed.empty());
  }

  TEST_METHOD(ALostFrameIsRecoveredByTheNextDelta)
  {
    // §5.3's whole recovery model: no retransmission, no ordering, just the next delta from an
    // older baseline. The link drops a third of everything for a while and the client's picture
    // still catches up with the simulation's.
    Match match;
    const Outpost::ObjectId walker = DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(8);
    Assert::IsTrue(match.client.State() == Outpost::ClientState::Playing);

    match.network.SetFaults({330, 0, 0, 0, 0});
    for (std::uint32_t step = 0; step < 30; ++step)
    {
      match.sim.Objects().FindDevice(walker)->x += Outpost::SUBUNITS_PER_WIRE_UNIT;
      match.Pump(2);
    }
    match.network.SetFaults({});
    match.Pump(12);

    Assert::IsTrue(match.network.Statistics().dropped > 0, L"the link really did lose frames");
    Assert::AreEqual(Outpost::WireFromSubunits(match.sim.Objects().FindDevice(walker)->x), match.sink.held[walker.value].x,
                     L"and the picture caught up regardless");
  }

  TEST_METHOD(AnEventOutlivesTheFrameThatDroppedItAndReachesItsCommanderExactlyOnce)
  {
    // m1-vertical-slice/C9's whole claim, and the one place §5.3's two halves meet. "State
    // survives a drop and events do not" is the bargain the frame format makes with ITSELF: a
    // frame carries the events of its own interval and a lost frame loses them. That is right for
    // the FORMAT and wrong for the HOST, because the host knows exactly which frame a client
    // acknowledged and therefore exactly which events it has never seen. So the queue lives on the
    // host side of the wire: an event waits in it until a frame that carried it is acknowledged,
    // and the client, which still holds nothing and re-sends nothing, sees each one once.
    //
    // A THIRD OF EVERYTHING IS LOST WHILE THE SHOOTING HAPPENS, which is the condition that makes
    // the difference visible at all: with a clean link every frame is acknowledged and the queue
    // is emptied as fast as it fills, so the case has to be run over a link that drops.
    Match match;
    const Outpost::ObjectId shooter = DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(8);
    Assert::IsTrue(match.client.State() == Outpost::ClientState::Playing);

    match.network.SetFaults({330, 0, 0, 0, 0});
    std::uint32_t produced = 0;
    for (std::uint32_t volley = 0; volley < 40; ++volley)
    {
      Outpost::Event shot{};
      shot.kind = Outpost::EventKind::Shot;
      shot.source = shooter.value;
      shot.sourceKind = shooter.kind;
      shot.x = static_cast<std::int32_t>(volley); // Distinct, so a duplicate is a duplicate of a KNOWN one
      shot.tick = match.sim.Tick();
      match.host.Events().push_back(shot);
      ++produced;
      match.Pump(2);
    }
    match.network.SetFaults({});
    match.Pump(24);

    Assert::IsTrue(match.network.Statistics().dropped > 0, L"the link really did lose frames");

    std::vector<std::int32_t> arrived;
    for (const Outpost::Frame& frame : match.sink.frames)
    {
      for (const Outpost::Event& event : frame.events)
      {
        if (event.kind == Outpost::EventKind::Shot)
        {
          arrived.push_back(event.x);
        }
      }
    }
    Assert::AreEqual(static_cast<std::size_t>(produced), arrived.size(), L"every shot reached him, and not one of them twice");
    std::sort(arrived.begin(), arrived.end());
    for (std::uint32_t volley = 0; volley < produced; ++volley)
    {
      Assert::AreEqual(static_cast<std::int32_t>(volley), arrived[volley], L"and they are the shots that were fired");
    }
  }

  TEST_METHOD(AnEventNobodyEverAcknowledgesIsForgottenRatherThanQueuedForEver)
  {
    // THE CAP, which is what keeps the queue from being a way to run a host out of memory. A
    // client that has stopped acknowledging is either gone or about to be given up on; until the
    // grace runs out the host goes on publishing to it, and without a ceiling every shot of the
    // match would pile up behind an acknowledgement that is never coming. MAX_PENDING_EVENTS the
    // oldest go, because a shot nobody drew two seconds ago is worth less than the one being fired
    // now - the queue is a redelivery window and not a log.
    Match match;
    const Outpost::ObjectId shooter = DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(8);

    // Nothing the client says gets back, so it acknowledges nothing from here on; the host keeps
    // publishing full frames to a baseline that never arrives.
    match.network.SetFaults({1000, 0, 0, 0, 0});
    const std::size_t before = match.sink.frames.size();
    for (std::uint32_t volley = 0; volley < Outpost::MAX_PENDING_EVENTS + 200; ++volley)
    {
      Outpost::Event shot{};
      shot.kind = Outpost::EventKind::Shot;
      shot.source = shooter.value;
      shot.sourceKind = shooter.kind;
      shot.x = static_cast<std::int32_t>(volley);
      shot.tick = match.sim.Tick();
      match.host.Events().push_back(shot);
      match.Pump(2);
    }
    Assert::AreEqual(before, match.sink.frames.size(), L"the client heard nothing at all while the link was down");

    match.network.SetFaults({});
    match.Pump(12);
    Assert::IsTrue(match.sink.frames.size() > before, L"and is talking again");

    std::vector<std::int32_t> arrived;
    for (std::size_t index = before; index < match.sink.frames.size(); ++index)
    {
      for (const Outpost::Event& event : match.sink.frames[index].events)
      {
        if (event.kind == Outpost::EventKind::Shot)
        {
          arrived.push_back(event.x);
        }
      }
    }
    Assert::IsFalse(arrived.empty(), L"what was still in the window arrived");
    Assert::IsTrue(arrived.size() <= Outpost::MAX_PENDING_EVENTS, L"and no more than the window holds");
    const auto oldest = *std::min_element(arrived.begin(), arrived.end());
    Assert::IsTrue(oldest > 0, L"the oldest shots were the ones dropped, not the newest");
  }

  TEST_METHOD(AClientWhoseBaselineHasAgedOutGetsAFullFrame)
  {
    // The history is 32 frames. A client that acknowledges nothing for longer than that cannot be
    // sent a delta, and the answer is the same path a join takes.
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(8);
    const std::uint32_t fullAfterJoin = match.host.Statistics().fullFrames;

    // Everything the client would send back is lost, so its acknowledgement never moves.
    match.network.SetFaults({1000, 0, 0, 0, 0});
    match.Pump(2 * static_cast<std::uint32_t>(Outpost::FRAME_HISTORY) + 20);
    match.network.SetFaults({});
    match.Pump(8);

    Assert::IsTrue(match.host.Statistics().fullFrames > fullAfterJoin, L"the baseline aged out and a full frame followed");
  }

  TEST_METHOD(AFrameTooLargeForADatagramIsSplitAndPutBackTogether)
  {
    Match match;
    for (std::uint32_t index = 0; index < 120; ++index)
    {
      DeviceAt(match.sim, 0, 16 + index % 20, 16 + index / 20);
    }
    Reveal(match.sim, 0, 10, 10, 30);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(10);

    Assert::IsTrue(match.host.Statistics().largestFrameBytes > Neuron::MAX_DATAGRAM_PAYLOAD_BYTES, L"the frame needed splitting");
    Assert::IsTrue(match.host.Statistics().fragments > 0);
    Assert::AreEqual(static_cast<std::size_t>(120), match.sink.held.size(), L"and every device arrived");
    Logger::WriteMessage(("    measured: a full frame of 120 devices is " + std::to_string(match.host.Statistics().largestFrameBytes) +
                          " bytes in " + std::to_string(match.host.Statistics().fragments) + " fragments\n")
                           .c_str());
  }

  TEST_METHOD(AnOrderReachesTheSimulationForTheNextTick)
  {
    Match match;
    const Outpost::ObjectId walker = DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(8);

    Outpost::Order stop{};
    stop.tick = match.sim.Tick() + 1;
    stop.seat = 0;
    stop.kind = Outpost::OrderKind::Stop;
    stop.operands = {static_cast<std::int32_t>(walker.value), 0, 0, 0};
    Assert::IsTrue(match.client.Submit(stop));
    match.Pump(10);
    Assert::AreEqual(static_cast<std::uint32_t>(1), match.host.Statistics().ordersApplied);
  }

  TEST_METHOD(AnOrderClaimingAnotherSeatIsRefusedByTheHost)
  {
    // Shape, not rules: "he does not own that device" is stage 1's judgement, but "he is not that
    // commander" is a client lying about who it is, and it never reaches the simulation.
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(8);

    Outpost::Order chat{};
    chat.tick = match.sim.Tick() + 1;
    chat.seat = 1; // not his seat
    chat.kind = Outpost::OrderKind::Chat;
    Assert::IsTrue(match.client.Submit(chat));
    match.Pump(10);
    Assert::AreEqual(static_cast<std::uint32_t>(0), match.host.Statistics().ordersApplied);
    Assert::AreEqual(static_cast<std::uint32_t>(1), match.host.Statistics().ordersRefusedShape);
  }

  /// m1-vertical-slice/N4. The host latches a seat's rejections every tick rather than reading them
  /// at publish time, and this is why: the simulation clears them at the start of every stage 1 and
  /// a publish is due on even ticks only, so half of them would be judged and thrown away.
  TEST_METHOD(ARefusalReachesTheNextFrameWhicheverTickJudgedIt)
  {
    bool sawOdd = false;
    bool sawEven = false;
    for (const std::uint32_t offset : {std::uint32_t{8}, std::uint32_t{9}})
    {
      Match match;
      DeviceAt(match.sim, 0, 16, 16);
      Reveal(match.sim, 0, 14, 14, 8);
      match.client.SendJoin(CONTENT, 1, "owner", 0);
      match.Pump(offset);

      // A device this seat does not own, which stage 1 refuses and Net does not: the shape is legal.
      Outpost::Order stop{};
      stop.tick = match.sim.Tick() + 1;
      stop.seat = 0;
      stop.kind = Outpost::OrderKind::Stop;
      stop.operands = {999999, 0, 0, 0};
      Assert::IsTrue(match.client.Submit(stop));

      // One tick at a time, so that the tick stage 1 judged it on is observable at all.
      std::uint32_t judgedOnTick = 0;
      Outpost::RejectReason judgedReason = Outpost::RejectReason::Accepted;
      for (std::uint32_t step = 0; step < 12; ++step)
      {
        match.sim.Advance();
        const std::span<const Outpost::Seat> seats = match.sim.Seats();
        if (judgedOnTick == 0 && !seats[0].rejections.empty())
        {
          judgedOnTick = match.sim.Tick();
          judgedReason = seats[0].rejections.front().reason;
        }
        match.network.Host().Poll();
        match.host.Advance(match.sim.Tick());
        match.clientEnd.Poll();
        match.client.Advance(match.sim.Tick(), match.sink);
      }

      Assert::AreNotEqual(std::uint32_t{0}, judgedOnTick, L"stage 1 refused the order at all");
      Assert::IsTrue(judgedReason != Outpost::RejectReason::Accepted);
      if (judgedOnTick % 2 == 1)
      {
        sawOdd = true;
      }
      else
      {
        sawEven = true;
      }

      Assert::IsFalse(match.sink.frames.empty(), L"the client was sent frames");
      const Outpost::SeatState& seat = match.sink.frames.back().seat;
      Assert::AreEqual(std::uint16_t{1}, seat.rejectSequence, L"the seat's first and only refusal");
      Assert::AreEqual(static_cast<std::uint8_t>(Outpost::OrderKind::Stop), seat.rejectKind);
      Assert::AreEqual(static_cast<std::uint8_t>(judgedReason), seat.rejectReason, L"the reason stage 1 gave");
    }

    // The point of running it twice. If both offsets landed on the same parity the assertions above
    // would still pass while proving only half of what this test is for.
    Assert::IsTrue(sawOdd, L"one of the two runs was judged on an odd tick");
    Assert::IsTrue(sawEven, L"and one on an even tick");
  }

  /// The reason the record carries a sequence and not just a kind and a reason: two identical
  /// refusals are equal by value, and the operator has to see the second one.
  TEST_METHOD(TwoIdenticalRefusalsAreTwoSequences)
  {
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(8);

    const auto askForWhatHeCannotHave = [&match]()
    {
      Outpost::Order stop{};
      stop.tick = match.sim.Tick() + 1;
      stop.seat = 0;
      stop.kind = Outpost::OrderKind::Stop;
      stop.operands = {999999, 0, 0, 0};
      Assert::IsTrue(match.client.Submit(stop));
      match.Pump(10);
      Assert::IsFalse(match.sink.frames.empty());
      return match.sink.frames.back().seat;
    };

    const Outpost::SeatState first = askForWhatHeCannotHave();
    const Outpost::SeatState second = askForWhatHeCannotHave();

    Assert::AreEqual(first.rejectKind, second.rejectKind, L"the same order");
    Assert::AreEqual(first.rejectReason, second.rejectReason, L"refused for the same reason");
    Assert::AreNotEqual(first.rejectSequence, second.rejectSequence, L"and it is a second refusal, not the first again");
  }

  TEST_METHOD(AClientMayWatchASeatThatIsAiAndMayNotWatchOneAHumanHolds)
  {
    // m1-vertical-slice/G2's observer. The capture is a match of two scripted commanders, and a
    // capture with no client has nothing to draw: the renderer draws a Replica and only a client
    // fills one. FreeSeat hands a joiner only a seat whose kind is Human, so the all-AI lobby that
    // G1 asked for refuses the join outright - which is what this exists to get past.
    Match match(LobbyWithAnAiSeat());
    DeviceAt(match.sim, 1, 16, 16);
    Reveal(match.sim, 1, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "watcher", 0, 1);
    match.Pump(8);

    Assert::IsTrue(match.client.State() == Outpost::ClientState::Playing, L"watching the scripted seat was allowed");
    Assert::IsTrue(match.client.Observing());
    Assert::AreEqual(std::uint8_t{1}, match.client.Seat(), L"and it watches the seat it asked for");
    Assert::AreEqual(static_cast<std::uint32_t>(1), match.host.Statistics().observers);
    Assert::AreEqual(static_cast<std::uint32_t>(0), match.host.Statistics().joins, L"it is not a player joining");

    // IT IS NOT THE SEAT IT WATCHES. Everything that asks who is sitting in seat 1 has to go on
    // saying nobody, or the next joiner is refused a seat that is free and the interface calls a
    // scripted commander a human.
    Assert::IsTrue(match.host.SeatState(1) == Outpost::SeatConnection::Open, L"nobody is playing the AI seat");
    Assert::IsNull(match.host.ClientForSeat(1));
    Assert::IsFalse(match.sink.frames.empty(), L"and it is sent that seat's frames all the same");
    Assert::AreEqual(static_cast<std::size_t>(1), match.sink.frames[0].createdDevices.size(), L"what the AI commander can see");

    // A SEAT A HUMAN HOLDS IS REFUSED, and the rule is on the seat's KIND rather than on whether
    // anyone is connected to it: nobody has joined seat 0 in this match at all, and it is still
    // refused, because a human's seat is still his while his rejoin grace runs.
    Neuron::Transport& secondEnd = match.network.Connect();
    Outpost::Client second(secondEnd, LIVENESS, 0);
    Recorder secondSink;
    second.SendJoin(CONTENT, 2, "intruder", 0, 0);
    for (std::uint32_t pass = 0; pass < 8; ++pass)
    {
      match.network.Host().Poll();
      match.host.Advance(match.sim.Tick());
      secondEnd.Poll();
      second.Advance(match.sim.Tick(), secondSink);
    }
    Assert::IsTrue(second.State() == Outpost::ClientState::Refused, L"a human's seat may not be watched");
    Assert::IsTrue(second.Refusal() == Outpost::RefusalReason::NoSeat);
    Assert::IsTrue(secondSink.frames.empty(), L"and a refused watcher is sent nothing at all");
  }

  TEST_METHOD(AnObserverSeesTheSeatsFogAndNothingOutsideItAndItsOrdersAreRefused)
  {
    // The two halves that make watching SAFE. The first is §5.2 holding for an observer exactly as
    // NetTests already asserts it for a player: it sees what that commander sees and no more, so a
    // capture is not a wallhack and neither is a spectator. The second is that it cannot play the
    // seat it watches.
    Match match(LobbyWithAnAiSeat());
    const Outpost::ObjectId seen = DeviceAt(match.sim, 1, 16, 16);
    const Outpost::ObjectId hidden = DeviceAt(match.sim, 0, 90, 90);
    Reveal(match.sim, 1, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "watcher", 0, 1);
    match.Pump(8);
    Assert::IsTrue(match.client.State() == Outpost::ClientState::Playing);

    Assert::IsTrue(match.sink.held.contains(seen.value), L"the seat's own device");
    Assert::IsFalse(match.sink.held.contains(hidden.value), L"and not one it cannot see");
    const Outpost::Frame& full = match.sink.frames.front();
    Assert::IsFalse(full.fog.empty(), L"the fog of the seat it watches came with the full frame");

    // ITS ORDERS ARE REFUSED AND NEVER REACH STAGE 1. The order is a well-formed one for the seat
    // it is watching, so nothing but the watching refuses it.
    Outpost::Order order{};
    order.seat = 1;
    order.kind = Outpost::OrderKind::Move;
    order.operands[0] = static_cast<std::int32_t>(seen.value);
    order.operands[1] = 20 * CELL;
    order.operands[2] = 20 * CELL;
    Assert::IsTrue(match.client.Submit(order));
    const std::int32_t before = match.sim.Objects().FindDevice(seen)->destinationX;
    match.Pump(8);

    Assert::AreEqual(static_cast<std::uint32_t>(1), match.host.Statistics().ordersRefusedObserver);
    Assert::AreEqual(static_cast<std::uint32_t>(0), match.host.Statistics().ordersApplied, L"it never reached stage 1");
    Assert::AreEqual(before, match.sim.Objects().FindDevice(seen)->destinationX, L"and the device was not sent anywhere");

    // AND IT IS TOLD SO, through the latch every refusal uses.
    const Outpost::SeatState& told = match.sink.frames.back().seat;
    const std::uint16_t itsOwnRefusal = told.rejectSequence;
    Assert::AreNotEqual(std::uint16_t{0}, itsOwnRefusal, L"a refusal was reported");
    Assert::AreEqual(static_cast<std::uint8_t>(Outpost::RejectReason::NotOwned), told.rejectReason,
                     L"it owns nothing in the seat it watches");
    Assert::AreEqual(static_cast<std::uint8_t>(Outpost::OrderKind::Move), told.rejectKind);

    // AND IT IS TOLD NOTHING ABOUT THE COMMANDER'S OWN CLICKS. A refusal answers the client that
    // sent the order, and an observer sent none of the seat's; the latch is where its OWN refused
    // orders are reported, so folding the seat's into it would overwrite that with an answer to a
    // question it never asked. The refusal below is a real one - stage 1 judges it, exactly as
    // ARefusalReachesTheNextFrameWhicheverTickJudgedIt asserts for a player.
    Outpost::Order theSeatsOwn{};
    theSeatsOwn.tick = match.sim.Tick() + 1;
    theSeatsOwn.seat = 1;
    theSeatsOwn.kind = Outpost::OrderKind::Stop;
    theSeatsOwn.operands = {999999, 0, 0, 0};
    match.sim.Submit(theSeatsOwn);
    bool judged = false;
    for (std::uint32_t step = 0; step < 12 && !judged; ++step)
    {
      match.Pump(1);
      judged = judged || !match.sim.Seats()[1].rejections.empty();
    }
    Assert::IsTrue(judged, L"stage 1 refused the seat's own order");
    match.Pump(8);
    Assert::AreEqual(itsOwnRefusal, match.sink.frames.back().seat.rejectSequence,
                     L"and the watcher was told nothing new: the seat's refusal is not its own");
  }

  TEST_METHOD(AWatcherThatGoesSilentIsGoneAndTheSeatItWatchedNeverNoticed)
  {
    // NOTHING WAITS FOR AN OBSERVER. The rejoin grace exists so a commander's seat is held while
    // he reconnects (GameDesign.md §10); a client that was only watching holds no seat, so its
    // silence means it is gone and not that a commander has dropped. Getting this wrong is how an
    // observer would corrupt the lifecycle of the seat it watched - the one thing it must not
    // touch - by pushing it to UnderAi, or by being published to for the rest of the match.
    Match match(LobbyWithAnAiSeat());
    DeviceAt(match.sim, 1, 16, 16);
    Reveal(match.sim, 1, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "watcher", 0, 1);
    match.Pump(8);
    Assert::IsTrue(match.client.State() == Outpost::ClientState::Playing);
    const std::uint32_t whileWatching = match.host.Statistics().framesPublished;
    Assert::IsTrue(whileWatching > 0);

    // The link goes one way: the host keeps publishing and hears nothing back.
    match.network.SetFaults({0, 0, 0, 0, 0});
    // Well past five seconds of quiet plus the lobby's grace, which is what Net/Host.cpp waits.
    for (std::uint32_t tick = 0; tick < 400 + 2 * Lobby().rejoinGraceTicks; ++tick)
    {
      match.sim.Advance();
      match.network.Host().Poll();
      match.host.Advance(match.sim.Tick());
      // The client is not advanced at all, so it never answers.
    }

    Assert::AreEqual(static_cast<std::uint32_t>(0), match.host.Statistics().droppedToAi, L"no commander dropped");
    Assert::IsTrue(match.host.SeatState(1) == Outpost::SeatConnection::Open, L"the AI seat is what it always was");
    const std::uint32_t afterItWentQuiet = match.host.Statistics().framesPublished;
    match.Pump(20, false);
    Assert::AreEqual(afterItWentQuiet, match.host.Statistics().framesPublished, L"and nothing is published to it any more");
    // AND IT IS LET GO RATHER THAN KEPT MARKED GONE, which is invisible in one match and is a leak
    // over a long one: a spectator that reconnects leaves an entry behind every time.
    Assert::AreEqual(std::size_t{0}, match.host.ConnectedClients(), L"the host is holding nothing for it");
  }

  TEST_METHOD(TheBytesAFrameTakesAtAHundredAndAtSixHundredObjects)
  {
    // What ADR-012 records, measured rather than estimated. A full frame is everything the
    // commander can see; a delta is what moved since he acknowledged the last one, which is the
    // number that decides whether this protocol fits a broadband link.
    for (const std::uint32_t objects : {std::uint32_t{100}, std::uint32_t{600}})
    {
      Match match;
      std::vector<Outpost::ObjectId> devices;
      devices.reserve(objects);
      for (std::uint32_t index = 0; index < objects; ++index)
      {
        devices.push_back(DeviceAt(match.sim, 0, 10 + index % 40, 10 + index / 40));
      }
      Reveal(match.sim, 0, 5, 5, 60);
      match.client.SendJoin(CONTENT, 1, "owner", 0);
      match.Pump(12);
      Assert::AreEqual(static_cast<std::size_t>(objects), match.sink.held.size(), L"every one of them arrived");
      const std::uint32_t full = match.host.Statistics().largestFrameBytes;

      // Half of them move a step, which is the shape of a battle rather than of a parade.
      for (std::uint32_t index = 0; index < objects; index += 2)
      {
        match.sim.Objects().FindDevice(devices[index])->x += 2 * Outpost::SUBUNITS_PER_WIRE_UNIT;
      }
      match.Pump(4);
      const std::uint32_t delta = match.host.LastFrameBytes(0);

      Logger::WriteMessage(("    measured: at " + std::to_string(objects) + " visible objects a full frame is " + std::to_string(full) +
                            " bytes and a delta with half of them moving is " + std::to_string(delta) + " bytes (" +
                            std::to_string(delta * 10 / 1024) + " KB/s at 10 Hz)\n")
                             .c_str());
      Assert::IsTrue(delta < full, L"a delta is smaller than the frame it is a delta from");
    }
  }

  TEST_METHOD(ASilentClientGoesUnderAiControlAndComesBackOnARejoin)
  {
    Match match;
    DeviceAt(match.sim, 0, 16, 16);
    Reveal(match.sim, 0, 14, 14, 8);
    match.client.SendJoin(CONTENT, 1, "owner", 0);
    match.Pump(8);
    Assert::IsTrue(match.host.SeatState(0) == Outpost::SeatConnection::Playing);

    // He stops answering: everything he sends is lost from here on.
    match.network.SetFaults({1000, 0, 0, 0, 0});
    match.Pump(200);
    Assert::IsTrue(match.host.SeatState(0) == Outpost::SeatConnection::UnderAi, L"the grace period ran out");
    Assert::AreEqual(static_cast<std::uint32_t>(1), match.host.Statistics().droppedToAi);

    // He comes back with the same token and takes the seat again, and gets a full frame with it.
    match.network.SetFaults({});
    const std::uint32_t fullBefore = match.host.Statistics().fullFrames;
    match.client.SendJoin(CONTENT, 1, "owner", match.sim.Tick());
    match.Pump(10);
    Assert::IsTrue(match.host.SeatState(0) == Outpost::SeatConnection::Playing, L"the same token is the same seat (§5.4)");
    Assert::AreEqual(static_cast<std::uint32_t>(1), match.host.Statistics().rejoins);
    Assert::IsTrue(match.host.Statistics().fullFrames > fullBefore, L"and nothing he held was trusted");
  }
};

} // namespace NetTests
