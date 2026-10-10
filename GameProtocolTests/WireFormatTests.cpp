#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameProtocolTests
{
namespace
{
// A snapshot with every field set, each to a value of its own, so that a field the wire dropped or swapped shows.
Outpost::Snapshot FullSnapshot()
{
  Outpost::Snapshot snapshot{.tick = 0x0102030405060708ull, .player = Outpost::PlayerId{2}};
  snapshot.entities.push_back(
    {.id = Outpost::EntityId{11},
     .kind = Outpost::EntityKind::Structure,
     .owner = Outpost::PlayerId{2},
     .design = Outpost::DesignId{12},
     .hull = Outpost::HullId{13},
     .drive = Outpost::DriveId{14},
     .weapon = Outpost::WeaponId{15},
     .module = Outpost::ModuleId{16},
     .role = Outpost::ShipRole::Constructor,
     .structure = Outpost::StructureKind::Relay,
     .position = {.xMeters = -17.5f, .zMeters = 18.25f},
     .headingRadians = 1.9f,
     .radiusMeters = 20.0f,
     .hitPointsHundredths = -21,
     .maxHitPointsHundredths = 22,
     .builtPermille = 23,
     .level = 3,
     .upgradePermille = 417,
     .shipyardNumber = 24,
     .shipsBuilt = 25,
     .queue = {{.role = Outpost::ShipRole::Warship, .design = Outpost::DesignId{26}}, {.role = Outpost::ShipRole::Constructor}},
     .research = {Outpost::ResearchTopicId{27}, Outpost::ResearchTopicId{28}},
     .jobPermille = 29,
     .secondJobPermille = 291,
     .remembered = true,
     .lastSeenTick = 0x1'0000'0007ull,
     .sightMeters = 30.5f,
     .standing = Outpost::StandingOrder::Patrol,
     .retreat = Outpost::RetreatThreshold::Half,
     .retreating = true,
     .oreReserveHundredths = -31'000'000'000ll,
     .salvageOre = 750,
     .salvageTopic = Outpost::ResearchTopicId{97},
     .salvagePermille = 333});
  snapshot.entities.push_back({.id = Outpost::EntityId{32}, .kind = Outpost::EntityKind::Derelict});
  snapshot.shots.push_back({.shooter = Outpost::EntityId{33},
                            .target = Outpost::EntityId{34},
                            .weapon = Outpost::WeaponId{35},
                            .from = {.xMeters = 36.0f},
                            .to = {.zMeters = 37.0f},
                            .splashRadiusMeters = 38.0f,
                            .gun = 2});
  snapshot.destroyed.push_back({.id = Outpost::EntityId{39},
                                .kind = Outpost::EntityKind::Structure,
                                .structure = Outpost::StructureKind::MiningRig,
                                .owner = Outpost::PlayerId{1},
                                .hull = Outpost::HullId{40},
                                .position = {.xMeters = 41.0f, .zMeters = 42.0f},
                                .headingRadians = 43.0f,
                                .radiusMeters = 44.0f});
  snapshot.ore = 45;
  snapshot.oreIncomeHundredthsPerSecond = 46;
  snapshot.designs.push_back({.id = Outpost::DesignId{47},
                              .nameUtf8 = "Medium+Fusion+Missile Rack",
                              .hull = Outpost::HullId{48},
                              .drive = Outpost::DriveId{49},
                              .weapon = Outpost::WeaponId{50},
                              .module = Outpost::ModuleId{51},
                              .cost = 52,
                              .retreat = Outpost::RetreatThreshold::Never});
  snapshot.mapSizeMeters = 5000.0f;
  snapshot.structureTypes.push_back({.structure = Outpost::StructureKind::DefensePlatform,
                                     .nameUtf8 = "Defense Platform",
                                     .radiusMeters = 53.0f,
                                     .buildable = true,
                                     .cost = 54,
                                     .levels = {{.cost = 150,
                                                 .buildSeconds = 30.5,
                                                 .maxHitPointsHundredths = 300'000,
                                                 .opensTier = 2,
                                                 .researchSlots = 2,
                                                 .prerequisites = {Outpost::ResearchTopicId{6}},
                                                 .nodes = 6,
                                                 .guns = 2,
                                                 .commandPoints = 30}}});
  snapshot.constructorCost = 55;
  snapshot.constructorBuildSeconds = 55.5;
  snapshot.hulls.push_back({.id = Outpost::HullId{56},
                            .nameUtf8 = "Small",
                            .hitPointsHundredths = 57,
                            .armorHundredths = 58,
                            .speedMetersPerSecond = 59.5,
                            .turnRateDegreesPerSecond = 60.5,
                            .footprintRadiusMeters = 61.5,
                            .cost = 62,
                            .buildSeconds = 63.5,
                            .available = true,
                            .shipyardLevel = 2,
                            .commandPoints = 4});
  snapshot.drives.push_back({.id = Outpost::DriveId{64},
                             .nameUtf8 = "Ion",
                             .speedFactor = 65.5,
                             .hitPointsFactor = 66.5,
                             .turnRateFactor = 67.5,
                             .cost = 68,
                             .available = true});
  snapshot.weapons.push_back({.id = Outpost::WeaponId{69},
                              .nameUtf8 = "Laser",
                              .damageHundredths = 70,
                              .fireIntervalSeconds = 71.5,
                              .rangeMeters = 72.5,
                              .splashRadiusMeters = 73.5,
                              .cost = 74,
                              .available = true});
  snapshot.modules.push_back(
    {.id = Outpost::ModuleId{75}, .nameUtf8 = "Sensor Array", .sightMeters = 76.5, .speedFactor = 0.75, .cost = 77, .available = true});
  snapshot.research.push_back({.id = Outpost::ResearchTopicId{78},
                               .nameUtf8 = "Fusion Drive",
                               .effectUtf8 = "Unlocks the Fusion drive.",
                               .cost = 79,
                               .researchSeconds = 80.5,
                               .prerequisites = {Outpost::ResearchTopicId{81}},
                               .researched = true,
                               .unlocksHull = Outpost::HullId{82},
                               .unlocksDrive = Outpost::DriveId{83},
                               .unlocksWeapon = Outpost::WeaponId{84},
                               .tier = 3,
                               .recovered = true});
  snapshot.shipyardBuildSpeedFactor = 1.25;
  snapshot.researchTier = 2;
  snapshot.nodeCap = 5;
  snapshot.commandPoints = 18;
  snapshot.fleetCap = 30;
  snapshot.matchOver = true;
  snapshot.winner = Outpost::PlayerId{1};
  snapshot.matchEndedTick = 85;
  snapshot.ending = Outpost::MatchEnding::Domination;
  snapshot.fogOfWar = true;
  snapshot.sectors.push_back({.id = 86,
                              .nameUtf8 = "Home",
                              .minXMeters = -87.0f,
                              .maxXMeters = 88.0f,
                              .minZMeters = -89.0f,
                              .maxZMeters = 90.0f,
                              .node = {.xMeters = 91.0f, .zMeters = 92.0f},
                              .adjacent = {93, 94},
                              .holder = Outpost::PlayerId{2},
                              .suppressed = true,
                              .cutOff = true,
                              .guarded = true});
  snapshot.tickets.push_back({.player = Outpost::PlayerId{1}, .tickets = 95});
  snapshot.startingTickets = 96;
  snapshot.drainIntervalSeconds = 3.25;
  snapshot.drainTicketsPerNodeDifference = 97;
  // An order fired (ADR-080), with every field set; and the last kind of event, a world's restarted seat (Phase 5 design §8),
  // and the tick its seat restarts at.
  const Outpost::EventView fired{.kind = Outpost::EventKind::OrderFired,
                                 .sector = 98,
                                 .position = {.xMeters = 9.5f, .zMeters = -9.5f},
                                 .subject = Outpost::EntityId{99},
                                 .structure = Outpost::StructureKind::RepairBay,
                                 .other = Outpost::PIRATES,
                                 .order = 100,
                                 .action = Outpost::ScheduledActionKind::BuildRig,
                                 .outcome = Outpost::OrderOutcome::Refused};
  snapshot.events.push_back(fired);
  snapshot.events.push_back({.kind = Outpost::EventKind::EmpireRestarted, .sector = 115});
  snapshot.restartTick = 116;
  snapshot.scheduled.push_back({.id = 101,
                                .ships = {Outpost::EntityId{102}},
                                .trigger = {.kind = Outpost::ScheduledTriggerKind::TimeOfDay, .utcSeconds = -103, .tick = 104},
                                .action = {.kind = Outpost::ScheduledActionKind::Attack, .target = Outpost::EntityId{105}},
                                .unlessCommandPoints = 106});
  snapshot.away = Outpost::AwayReport{.sinceTick = 107,
                                      .shipsBuilt = 108,
                                      .structuresBuilt = 109,
                                      .shipsLost = 110,
                                      .structuresLost = 111,
                                      .sectorsGained = {112},
                                      .sectorsLost = {113},
                                      .ordersFired = {fired}};
  snapshot.entities[0].scheduledOrder = 114;
  return snapshot;
}

Outpost::Message RoundTrip(const auto& _message)
{
  const std::vector<std::byte> bytes = Outpost::EncodeMessage(_message);
  Outpost::Message decoded = Outpost::DecodeMessage(bytes);
  // Encoding what was decoded gives the same bytes, so nothing was lost on the way.
  std::visit([&bytes](const auto& _decoded) { Assert::IsTrue(Outpost::EncodeMessage(_decoded) == bytes, L"the same bytes again"); },
             decoded);
  return decoded;
}
} // namespace

TEST_CLASS(WireFormatTests)
{
public:
  TEST_METHOD(CarriesEveryOrder)
  {
    const std::vector<Outpost::EntityId> ships{Outpost::EntityId{3}, Outpost::EntityId{4}};
    const std::vector<Outpost::Order> orders{
      Outpost::MoveCommand{.ships = ships, .destination = {.xMeters = 1.5f, .zMeters = -2.5f}},
      Outpost::AttackCommand{.ships = ships, .target = Outpost::EntityId{5}},
      Outpost::AttackMoveCommand{.ships = ships, .destination = {.xMeters = 6.0f}},
      Outpost::StopCommand{.ships = ships},
      Outpost::BuildStructureCommand{.constructors = ships, .structure = Outpost::StructureKind::Relay, .position = {.zMeters = 7.0f}},
      Outpost::RepairCommand{.constructors = ships, .target = Outpost::EntityId{8}},
      Outpost::QueueShipCommand{.producer = Outpost::EntityId{9}, .design = Outpost::DesignId{10}},
      Outpost::StartResearchCommand{.lab = Outpost::EntityId{11}, .topic = Outpost::ResearchTopicId{12}},
      Outpost::SaveDesignCommand{.design = Outpost::DesignId{13},
                                 .nameUtf8 = "Swarm",
                                 .hull = Outpost::HullId{14},
                                 .drive = Outpost::DriveId{15},
                                 .weapon = Outpost::WeaponId{16},
                                 .module = Outpost::ModuleId{17},
                                 .retreat = Outpost::RetreatThreshold::Half},
      Outpost::HoldSectorCommand{.ships = ships, .position = {.xMeters = 18.0f}},
      Outpost::PatrolCommand{.ships = ships, .destination = {.zMeters = 19.0f}},
      Outpost::UpgradeStructureCommand{.structure = Outpost::EntityId{21}},
      Outpost::SalvageCommand{.constructors = ships, .derelict = Outpost::EntityId{22}},
      Outpost::SetRetreatCommand{.ships = ships, .retreat = Outpost::RetreatThreshold::Quarter},
      Outpost::ScheduleOrderCommand{
        .ships = ships,
        .trigger = {.kind = Outpost::ScheduledTriggerKind::RigLost, .sector = 23, .utcSeconds = 1'790'000'000, .tick = 24},
        .action = {.kind = Outpost::ScheduledActionKind::BuildRig, .position = {.xMeters = 25.0f}, .target = Outpost::EntityId{26}},
        .unlessCommandPoints = 27}};
    Assert::AreEqual(std::variant_size_v<Outpost::Order>, orders.size(), L"every alternative once");
    for (size_t i = 0; i < orders.size(); ++i)
    {
      Assert::AreEqual(i, orders[i].index());
      const Outpost::Message decoded = RoundTrip(Outpost::Command{.player = Outpost::PlayerId{20}, .order = orders[i]});
      const auto& command = std::get<Outpost::Command>(decoded);
      Assert::AreEqual(std::uint32_t{20}, command.player.value);
      Assert::AreEqual(i, command.order.index());
    }

    const Outpost::Message scheduledMessage = RoundTrip(Outpost::Command{.order = orders[14]});
    const auto& scheduled = std::get<Outpost::ScheduleOrderCommand>(std::get<Outpost::Command>(scheduledMessage).order);
    Assert::IsTrue(scheduled.trigger == std::get<Outpost::ScheduleOrderCommand>(orders[14]).trigger, L"every field of a trigger");
    Assert::IsTrue(scheduled.action == std::get<Outpost::ScheduleOrderCommand>(orders[14]).action);
    Assert::IsTrue(scheduled.unlessCommandPoints == std::optional<std::int32_t>{27});

    const Outpost::Message save = RoundTrip(Outpost::Command{.order = orders[8]});
    const auto& design = std::get<Outpost::SaveDesignCommand>(std::get<Outpost::Command>(save).order);
    Assert::AreEqual(std::string("Swarm"), design.nameUtf8);
    Assert::AreEqual(std::uint32_t{17}, design.module.value);
    Assert::IsTrue(design.retreat == Outpost::RetreatThreshold::Half);
  }

  TEST_METHOD(CarriesEveryFieldOfASnapshot)
  {
    const Outpost::Message decoded = RoundTrip(FullSnapshot());
    const auto& snapshot = std::get<Outpost::Snapshot>(decoded);
    const Outpost::Snapshot expected = FullSnapshot();
    Assert::AreEqual(expected.tick, snapshot.tick);
    Assert::IsTrue(expected.entities == snapshot.entities, L"entities, with their queues and research");
    Assert::IsTrue(expected.sectors == snapshot.sectors);
    Assert::IsTrue(expected.tickets == snapshot.tickets);
    Assert::IsTrue(expected.events == snapshot.events, L"the tick's events");
    Assert::IsTrue(expected.scheduled == snapshot.scheduled, L"the player's scheduled orders");
    Assert::IsTrue(expected.away == snapshot.away, L"the report of a time away");
    Assert::IsTrue(snapshot.entities[0].oreReserveHundredths == std::optional<std::int64_t>{-31'000'000'000ll});
    Assert::IsFalse(snapshot.entities[1].oreReserveHundredths.has_value());
    Assert::IsTrue(snapshot.entities[0].upgradePermille == std::optional<std::int32_t>{417});
    Assert::IsFalse(snapshot.entities[1].upgradePermille.has_value());
    Assert::IsTrue(expected.structureTypes[0].levels == snapshot.structureTypes[0].levels, L"a kind's levels");
    Assert::AreEqual(2, snapshot.researchTier);
    Assert::AreEqual(5, snapshot.nodeCap);
    Assert::AreEqual(18, snapshot.commandPoints);
    Assert::AreEqual(30, snapshot.fleetCap);
    Assert::AreEqual(2, snapshot.hulls[0].shipyardLevel);
    Assert::AreEqual(4, snapshot.hulls[0].commandPoints);
    Assert::AreEqual(2, static_cast<int>(snapshot.shots[0].gun), L"the gun that fired");
    Assert::AreEqual(std::string("Unlocks the Fusion drive."), snapshot.research[0].effectUtf8);
    Assert::AreEqual(0.75, snapshot.modules[0].speedFactor);
    Assert::IsTrue(snapshot.ending == Outpost::MatchEnding::Domination);
    Assert::AreEqual(96, snapshot.startingTickets);
    Assert::IsTrue(snapshot.drainIntervalSeconds == 3.25 && snapshot.drainTicketsPerNodeDifference == 97, L"the drain (task UI3.3)");
    Assert::IsTrue(snapshot.events.size() == 2 && snapshot.events[1].kind == Outpost::EventKind::EmpireRestarted,
                   L"the last kind of event");
    Assert::AreEqual(std::uint64_t{116}, snapshot.restartTick.value_or(0), L"when a world's lost seat restarts");
    Assert::AreEqual(55.5, snapshot.constructorBuildSeconds, L"the Constructor's build time (ADR-068)");
    Assert::IsTrue(snapshot.entities[1].kind == Outpost::EntityKind::Derelict, L"the last kind of entity (ADR-074)");
    Assert::AreEqual(97u, snapshot.entities[0].salvageTopic.value);
    Assert::IsTrue(snapshot.research[0].recovered);
    Assert::IsTrue(snapshot.entities[0].retreat == Outpost::RetreatThreshold::Half && snapshot.entities[0].retreating, L"ADR-075");
    Assert::IsTrue(snapshot.designs[0].retreat == Outpost::RetreatThreshold::Never);
  }

  TEST_METHOD(CarriesHelloAndWelcome)
  {
    const Outpost::Message hello = RoundTrip(Outpost::HelloMessage{.player = Outpost::PlayerId{2}});
    Assert::AreEqual(Outpost::PROTOCOL_VERSION, std::get<Outpost::HelloMessage>(hello).protocolVersion);
    Assert::AreEqual(Outpost::WireLayoutHash(), std::get<Outpost::HelloMessage>(hello).layoutHash, L"this build's layout");
    Assert::AreEqual(std::uint32_t{2}, std::get<Outpost::HelloMessage>(hello).player.value);
    const Outpost::Message welcome = RoundTrip(Outpost::WelcomeMessage{.player = Outpost::PlayerId{1}});
    Assert::AreEqual(std::uint32_t{1}, std::get<Outpost::WelcomeMessage>(welcome).player.value);
  }

  // ADR-060: a hello's version and its layout's hash are its first two fields, which a server reads before it decodes the
  // rest, so that a hello of another build is told apart from a broken one; the hash is of every message's layout.
  TEST_METHOD(SaysHowItsMessagesAreLaidOut)
  {
    const std::string layout = Outpost::WireLayout();
    Assert::IsTrue(layout.starts_with("a(r4{u32,u64,"), L"the hello first, its version and its layout's hash first");
    std::uint64_t hash = 0xCBF29CE484222325ull;
    for (const char character : layout)
      hash = (hash ^ static_cast<std::uint8_t>(character)) * 0x100000001B3ull;
    Assert::AreEqual(hash, Outpost::WireLayoutHash());

    const std::vector<std::byte> hello = Outpost::EncodeMessage(Outpost::HelloMessage{.layoutHash = 0x0123456789ABCDEFull});
    Assert::AreEqual(Outpost::PROTOCOL_VERSION, Outpost::PeekHelloVersion(hello).value_or(0));
    Assert::AreEqual(std::uint64_t{0x0123456789ABCDEF}, Outpost::PeekHelloLayout(hello).value_or(0));
    Assert::IsFalse(Outpost::PeekHelloLayout(std::span(hello).first(12)).has_value(), L"cut short inside the hash");
    Assert::IsTrue(Outpost::PeekHelloVersion(std::span(hello).first(12)).has_value(), L"but not inside the version");
    const std::vector<std::byte> welcome = Outpost::EncodeMessage(Outpost::WelcomeMessage{.player = Outpost::PlayerId{1}});
    Assert::IsFalse(Outpost::PeekHelloLayout(welcome).has_value(), L"not a hello");
  }

  TEST_METHOD(RefusesAMessageCutShortOrRunOn)
  {
    const std::vector<std::byte> bytes = Outpost::EncodeMessage(FullSnapshot());
    for (size_t length = 0; length < bytes.size(); ++length)
    {
      const std::span<const std::byte> cut(bytes.data(), length);
      Assert::ExpectException<Neuron::Exception>([cut] { (void)Outpost::DecodeMessage(cut); });
    }
    std::vector<std::byte> runOn = bytes;
    runOn.push_back(std::byte{0});
    Assert::ExpectException<Neuron::Exception>([&runOn] { (void)Outpost::DecodeMessage(runOn); });
  }

  TEST_METHOD(RefusesValuesNoFieldCanTake)
  {
    // A build order to no constructors, byte by byte: the message's kind, the player, the order's alternative, the count
    // of constructors, the structure, and the position.
    const std::vector<std::byte> build = Outpost::EncodeMessage(
      Outpost::Command{.order = Outpost::BuildStructureCommand{.structure = Outpost::StructureKind::Relay, .position = {.xMeters = 1.0f}}});
    Assert::AreEqual(size_t{1 + 4 + 1 + 4 + 1 + 8}, build.size());
    Assert::IsTrue(build[10] == std::byte{5}, L"the Relay");

    const auto refused = [&build](size_t _offset, std::byte _value)
    {
      std::vector<std::byte> changed = build;
      changed[_offset] = _value;
      Assert::ExpectException<Neuron::Exception>([&changed] { (void)Outpost::DecodeMessage(changed); });
    };
    refused(0, std::byte{4});    // a fifth kind of message
    refused(5, std::byte{14});   // a fifteenth order
    refused(6, std::byte{0xFF}); // more constructors than bytes
    refused(10, std::byte{7});   // a structure past the Repair Bay, the last
  }
};
} // namespace GameProtocolTests
