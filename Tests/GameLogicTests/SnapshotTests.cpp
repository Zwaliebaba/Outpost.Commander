#include "pch.h"

#include "Sim.h"
#include "Snapshot.h"

#include "ByteReader.h"
#include "ByteWriter.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SimTests
{

namespace
{
/// The tables a match is played by. These suites exercise the simulation rather than the rules, so
/// an empty tree is the honest one: no row is read, and Q20's binding is still exercised, because
/// the snapshot carries this tree's hash and refuses any other.
const Outpost::ContentTree& NoContent()
{
  static const Outpost::ContentTree TREE{};
  return TREE;
}

Outpost::MatchSettings ThreeSeats()
{
  Outpost::MatchSettings settings{};
  settings.seed = 0x1234567890ull;
  settings.sizeClass = Outpost::SizeClass::Medium;
  settings.seatCount = 3;
  settings.baseLevel = Outpost::BaseLevel::Small;
  settings.powerLevel = Outpost::PowerLevel::High;
  settings.technologyTiers = 1;
  settings.victory = Outpost::VictoryCondition::Survival;
  settings.survivalTicks = 100000;
  settings.seats[0] = {Outpost::SeatKind::Human, 0};
  settings.seats[1] = {Outpost::SeatKind::Ai, 1};
  settings.seats[2] = {Outpost::SeatKind::Ai, 2};
  return settings;
}

Outpost::Order Chat(std::uint32_t _tick, std::uint8_t _seat)
{
  Outpost::Order order{};
  order.tick = _tick;
  order.seat = _seat;
  order.kind = Outpost::OrderKind::Chat;
  order.operands = {1, 2, 3, 4};
  return order;
}

/// The Sim a snapshot of _sim reads back as; a snapshot that does not read back fails the test here.
Outpost::Sim Reload(const Outpost::Sim& _sim)
{
  std::optional<Outpost::Sim> reloaded = Outpost::Snapshot::Read(Outpost::Snapshot::Write(_sim), NoContent());
  if (!reloaded.has_value())
  {
    Assert::Fail(L"the snapshot did not read back"); // noreturn, which is what the optional access below relies on
  }
  return *reloaded;
}

/// Fills a seat's every variable-length field, so that the snapshot's counts and bounds are
/// exercised rather than written as zero. The systems that will do this are S3, S5 and S6.
void Furnish(Outpost::Seat& _seat, std::uint32_t _salt)
{
  _seat.stockpileCapHundredths = 250000 + static_cast<std::int32_t>(_salt);
  _seat.researchComplete = {1, 4, 9, 16 + _salt};
  // The lab is part of the record now (m1-vertical-slice/S6): a destroyed lab loses its
  // progress, and a list that does not say whose progress it is cannot enforce that.
  _seat.researchActive = {{{3, Outpost::ObjectKind::Structure}, 7, 120}, {{4, Outpost::ObjectKind::Structure}, 11, 60 + _salt}};
  Outpost::DeviceDesign design{};
  design.chassis = 1 + _salt;
  design.drive = 2;
  design.modules = {3, 4, 0, 0, 0, 0, 0, 0};
  design.moduleCount = 2;
  _seat.designs.push_back(design);
  design.chassis = 9;
  design.moduleCount = 1;
  _seat.designs.push_back(design);
  _seat.deviceCount = 3 + _salt;
  _seat.deviceCap = 120;
  _seat.structureCount = 2;
  _seat.structureCap = 80;
  _seat.ghosts.Record({{40 + _salt, Outpost::ObjectKind::Structure}, 1, 5, 12, 13, 900});
  _seat.ghosts.Record({{41 + _salt, Outpost::ObjectKind::Structure}, 2, 6, 30, 31, 950});
  // A small grid rather than a landscape's: what is under test is that the counts and the states
  // survive the stream, and 64 cells exercise that as well as a million would.
  _seat.fog.Resize(8);
  const std::uint32_t lit = 3 + _salt;
  _seat.fog.AddViewer(lit % 8, lit / 8);
  _seat.fog.AddViewer(lit % 8, lit / 8);
  // Cell 9 is seen and then left: explored, which no viewer count can produce on its own.
  _seat.fog.AddViewer(1, 1);
  _seat.fog.RemoveViewer(1, 1);
}

/// One of each kind, so that every map, every record and the id counter are on the wire.
void Populate(Outpost::Sim& _sim)
{
  Outpost::Device device{};
  device.seat = 0;
  device.design = 1;
  device.x = 4096;
  device.y = 256;
  device.z = -2048;
  device.facing = 0x8000;
  device.hitPoints = 240;
  device.experience = 12;
  device.primaryOrder = Outpost::PrimaryOrder::AttackMove;
  device.destinationX = 8192;
  device.destinationZ = 1024;
  device.fire = Outpost::FireStance::HoldFire;
  device.retreat = Outpost::RetreatStance::AtQuarter;
  device.group = 5;
  device.reloadTicks = {5, 2, 0, 0, 0, 0, 0, 0};
  const Outpost::ObjectId shooter = _sim.Objects().Create(device);
  device.seat = 1;
  device.x = -4096;
  device.primaryOrder = Outpost::PrimaryOrder::Stop;
  const Outpost::ObjectId other = _sim.Objects().Create(device);

  Outpost::Structure structure{};
  structure.seat = 0;
  structure.design = 3;
  structure.cellX = 17;
  structure.cellY = 42;
  structure.y = 128;
  structure.state = Outpost::StructurePhase::Standing;
  structure.hitPoints = 600;
  structure.buildEffortHundredths = 10000;
  structure.modules = {1, 2, 3, 0};
  structure.moduleCount = 3;
  structure.working = other;
  structure.workRemainingTicks = 55;
  _sim.Objects().Create(structure);

  Outpost::Projectile projectile{};
  projectile.seat = 0;
  projectile.shooter = shooter;
  projectile.module = 4;
  projectile.x = 4200;
  projectile.y = 300;
  projectile.z = -2000;
  projectile.impactX = -4096;
  projectile.impactY = 256;
  projectile.impactZ = 0;
  // Longer than the fixture's fifty ticks, because stage 9 is real now: a shell with nine ticks
  // left lands on tick nine and is gone, and what this fixture is for is a shell IN FLIGHT
  // (m1-vertical-slice/S10). The same reason Furnish runs after the ticks rather than before.
  projectile.ticksToImpact = 900;
  projectile.hitPercent = 80;
  projectile.damage = 24;
  _sim.Objects().Create(projectile);

  Outpost::Feature feature{};
  feature.design = 2;
  feature.cellX = 60;
  feature.cellY = 61;
  feature.y = 96;
  feature.facing = 0x2000;
  _sim.Objects().Create(feature);

  Outpost::Wreck wreck{};
  wreck.seat = 1;
  wreck.origin = other;
  wreck.design = 1;
  wreck.x = -4000;
  wreck.y = 250;
  wreck.z = 100;
  wreck.facing = 0xC000;
  wreck.decayTicks = 300;
  const Outpost::ObjectId hulk = _sim.Objects().Create(wreck);
  // A removed object must not come back through the snapshot, and the counter must not rewind.
  Assert::IsTrue(_sim.Objects().Remove(hulk));
}

/// A match a little way in, with a surrendered seat, orders applied and dropped, and orders pending.
Outpost::Sim Busy()
{
  Outpost::Sim sim(ThreeSeats(), NoContent());
  Populate(sim);
  for (std::uint32_t tick = 1; tick <= 50; ++tick)
  {
    sim.Submit(Chat(tick, static_cast<std::uint8_t>(tick % 4)));
    if (tick == 20)
    {
      Outpost::Order surrender = Chat(tick, 2);
      surrender.kind = Outpost::OrderKind::Surrender;
      sim.Submit(surrender);
    }
    sim.Advance();
  }
  // Furnished after the ticks rather than before them. Stages 2, 3 and 4 OWN several of these
  // fields now - the counts, the caps, and a lab's progress, which m1-vertical-slice/S6 drops when
  // the lab is not a standing lab - so a fixture that filled them first would be measuring what
  // the tick left rather than what the stream carries.
  for (std::uint8_t seat = 0; seat < 3; ++seat)
  {
    Furnish(sim.SeatAt(seat), seat);
  }
  sim.Submit(Chat(60, 0));
  sim.Submit(Chat(55, 1));
  sim.Submit(Chat(55, 0));
  return sim;
}

} // namespace

TEST_CLASS(SnapshotTests)
{
public:
  TEST_METHOD(AReloadedSimIsIndistinguishableFromTheOriginal)
  {
    Outpost::Sim original = Busy();
    Logger::WriteMessage(
      (L"measured: the snapshot of the three-seat match is " + std::to_wstring(Outpost::Snapshot::Write(original).size()) + L" bytes")
        .c_str());
    Outpost::Sim reloaded = Reload(original);
    Assert::IsTrue(original.Settings() == reloaded.Settings());
    Assert::AreEqual(original.Tick(), reloaded.Tick());
    Assert::AreEqual(original.Hash(), reloaded.Hash());
    Assert::AreEqual(original.ComputeHash(), reloaded.ComputeHash());
    Assert::IsTrue(original.Stream().GetState() == reloaded.Stream().GetState());
    Assert::IsTrue(original.Seats().size() == reloaded.Seats().size());
    for (std::size_t seat = 0; seat < original.Seats().size(); ++seat)
    {
      Assert::IsTrue(original.Seats()[seat] == reloaded.Seats()[seat]);
    }
    Assert::IsTrue(original.Seats()[2].Defeated(), L"seat 2 surrendered");
    Assert::AreEqual(original.AppliedOrders(), reloaded.AppliedOrders());
    Assert::AreEqual(original.DroppedOrders(), reloaded.DroppedOrders());
    Assert::AreEqual(original.Finished(), reloaded.Finished());
    Assert::AreEqual(static_cast<int>(original.WinningAlliance()), static_cast<int>(reloaded.WinningAlliance()));
    Assert::AreEqual(original.PublishDue(), reloaded.PublishDue());
    Assert::IsTrue(original.Orders().Entries() == reloaded.Orders().Entries());
    Assert::AreEqual(original.Orders().NextArrival(), reloaded.Orders().NextArrival());
    // The pending orders apply in both, in the same order, on the same ticks.
    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      original.Advance();
      reloaded.Advance();
      Assert::AreEqual(original.Hash(), reloaded.Hash());
    }
    Assert::IsTrue(original.Orders().Empty());
    Assert::IsTrue(reloaded.Orders().Empty());
  }

  TEST_METHOD(APopulatedWorldRoundTripsWithItsIdsAndItsCounter)
  {
    const Outpost::Sim original = Busy();
    const Outpost::Sim reloaded = Reload(original);
    const Outpost::World& before = original.Objects();
    const Outpost::World& after = reloaded.Objects();
    for (const Outpost::ObjectKind kind : {Outpost::ObjectKind::Device, Outpost::ObjectKind::Structure, Outpost::ObjectKind::Projectile,
                                           Outpost::ObjectKind::Feature, Outpost::ObjectKind::Wreck})
    {
      Assert::AreEqual(before.Count(kind), after.Count(kind));
    }
    Assert::AreEqual(std::size_t{0}, after.Count(Outpost::ObjectKind::Wreck), L"the removed wreck did not come back");
    Assert::AreEqual(before.NextId(), after.NextId(), L"the counter is state: the next id must be the same one");

    std::vector<std::uint32_t> ids;
    before.ForEachDevice(
      [&ids, &after](Outpost::ObjectId _id, const Outpost::Device& _device)
      {
        ids.push_back(_id.value);
        const Outpost::Device* reloadedDevice = after.FindDevice(_id);
        Assert::IsNotNull(reloadedDevice, L"every id resolves in the reloaded world");
        Assert::IsTrue(_device == *reloadedDevice, L"and to a record equal field for field");
      });
    Assert::AreEqual(std::size_t{2}, ids.size());
    before.ForEachStructure([&after](Outpost::ObjectId _id, const Outpost::Structure& _structure)
                            { Assert::IsTrue(_structure == *after.FindStructure(_id)); });
    before.ForEachProjectile([&after](Outpost::ObjectId _id, const Outpost::Projectile& _projectile)
                             { Assert::IsTrue(_projectile == *after.FindProjectile(_id)); });
    before.ForEachFeature([&after](Outpost::ObjectId _id, const Outpost::Feature& _feature)
                          { Assert::IsTrue(_feature == *after.FindFeature(_id)); });
  }

  TEST_METHOD(TheHashMovesForASingleFieldOfEveryRecordAndEverySeat)
  {
    // The hash is what a determinism test compares, so a field the hash does not read is a field
    // two machines may disagree about in silence. One mutation apiece, each one must move it.
    const Outpost::Sim original = Busy();
    const std::uint64_t baseline = original.ComputeHash();

    const auto moved = [baseline](Outpost::Sim& _sim, const wchar_t* _what) { Assert::AreNotEqual(baseline, _sim.ComputeHash(), _what); };

    Outpost::Sim device = Busy();
    device.Objects().FindDevice({1, Outpost::ObjectKind::Device})->hitPoints += 1;
    moved(device, L"a device's hit points");

    Outpost::Sim reload = Busy();
    reload.Objects().FindDevice({1, Outpost::ObjectKind::Device})->reloadTicks[7] = 1;
    moved(reload, L"a device's last reload slot");

    Outpost::Sim structure = Busy();
    structure.Objects().FindStructure({3, Outpost::ObjectKind::Structure})->buildEffortHundredths -= 1;
    moved(structure, L"a structure's build progress");

    Outpost::Sim projectile = Busy();
    projectile.Objects().FindProjectile({4, Outpost::ObjectKind::Projectile})->impactZ += 1;
    moved(projectile, L"a projectile's impact");

    // The two fields version 13 added (m1-vertical-slice/S10): what a shell was fired with, and a
    // structure's weapon reload. A shell that hashed the same however hard it had been fired would
    // land for different damage on two hosts.
    Outpost::Sim shot = Busy();
    shot.Objects().FindProjectile({4, Outpost::ObjectKind::Projectile})->damage += 1;
    moved(shot, L"the damage a shell was fired with");

    Outpost::Sim aimed = Busy();
    aimed.Objects().FindProjectile({4, Outpost::ObjectKind::Projectile})->hitPercent += 1;
    moved(aimed, L"the chance a shell was fired with");

    Outpost::Sim emplacement = Busy();
    emplacement.Objects().FindStructure({3, Outpost::ObjectKind::Structure})->reloadTicks += 1;
    moved(emplacement, L"a structure's weapon reload");

    Outpost::Sim feature = Busy();
    feature.Objects().FindFeature({5, Outpost::ObjectKind::Feature})->facing += 1;
    moved(feature, L"a feature's facing");

    Outpost::Sim counter = Busy();
    counter.Objects().SetNextId(counter.Objects().NextId() + 1);
    moved(counter, L"the id counter, which decides every id still to be issued");

    Outpost::Sim removed = Busy();
    Assert::IsTrue(removed.Objects().Remove({1, Outpost::ObjectKind::Device}));
    moved(removed, L"a device removed");

    Outpost::Sim power = Busy();
    power.SeatAt(1).powerHundredths += 1;
    moved(power, L"a seat's power");

    Outpost::Sim research = Busy();
    research.SeatAt(1).researchActive[0].remainingTicks -= 1;
    moved(research, L"a seat's research in progress");

    Outpost::Sim design = Busy();
    design.SeatAt(2).designs[1].moduleCount = 2;
    moved(design, L"a seat's design");

    Outpost::Sim cap = Busy();
    cap.SeatAt(0).structureCap += 1;
    moved(cap, L"a seat's structure cap");

    Outpost::Sim ghost = Busy();
    {
      Outpost::Ghost later = ghost.SeatAt(0).ghosts.All()[1];
      later.seenTick += 1;
      ghost.SeatAt(0).ghosts.Record(later);
    }
    moved(ghost, L"a seat's ghost store");

    Outpost::Sim surrender = Busy();
    surrender.SeatAt(0).surrendered = true;
    moved(surrender, L"a seat's surrender, which the victory state alone does not say");

    Outpost::Sim standing = Busy();
    standing.SeatAt(1).victory = Outpost::VictoryState::Eliminated;
    moved(standing, L"where a seat stands (m1-vertical-slice/S11)");

    Outpost::Sim extracted = Busy();
    extracted.SeatAt(1).extractedHundredths += 1;
    moved(extracted, L"what a seat has extracted, which the survival clock is settled on");

    Outpost::Sim established = Busy();
    established.SeatAt(1).everHeldBase = !established.Seats()[1].everHeldBase;
    moved(established, L"whether a seat has ever held a base, which decides if it can be annihilated");

    Outpost::Sim fog = Busy();
    fog.SeatAt(2).fog.AddViewer(1, 1);
    moved(fog, L"one cell of a seat's fog");
  }

  TEST_METHOD(ASnapshotIsRefusedAgainstTablesItWasNotWrittenAgainst)
  {
    // The whole of Q20's answer: a match reloaded against different rules is a different match,
    // and the digest turns that from a divergence nobody notices into a refusal here.
    Outpost::ContentTree tables{};
    Outpost::ChassisDesc chassis{};
    chassis.id = "ChassisLight";
    chassis.hitPoints = 100;
    tables.components.chassis.push_back(chassis);

    Outpost::Sim sim(ThreeSeats(), tables);
    sim.Advance();
    const std::vector<std::byte> bytes = Outpost::Snapshot::Write(sim);

    Assert::IsTrue(Outpost::Snapshot::Read(bytes, tables).has_value(), L"the tables it was written against");
    Assert::IsFalse(Outpost::Snapshot::Read(bytes, NoContent()).has_value(), L"no tables at all");

    Outpost::ContentTree edited = tables;
    edited.components.chassis[0].hitPoints += 1;
    Assert::IsFalse(Outpost::Snapshot::Read(bytes, edited).has_value(), L"one number of one row changed");

    Outpost::ContentTree same = tables;
    Assert::IsTrue(Outpost::Snapshot::Read(bytes, same).has_value(), L"an equal tree, not the same object");
  }

  TEST_METHOD(WritingTheSameSimTwiceGivesTheSameBytes)
  {
    const Outpost::Sim sim = Busy();
    Assert::IsTrue(Outpost::Snapshot::Write(sim) == Outpost::Snapshot::Write(sim));
    const Outpost::Sim again = Busy();
    Assert::IsTrue(Outpost::Snapshot::Write(sim) == Outpost::Snapshot::Write(again), L"the same history must give the same snapshot");
  }

  TEST_METHOD(ATruncatedSnapshotIsRefusedAtEveryLength)
  {
    const std::vector<std::byte> bytes = Outpost::Snapshot::Write(Busy());
    for (std::size_t length = 0; length < bytes.size(); ++length)
    {
      if (Outpost::Snapshot::Read(std::span<const std::byte>(bytes.data(), length), NoContent()).has_value())
      {
        Assert::Fail((L"a snapshot cut to " + std::to_wstring(length) + L" bytes read back").c_str());
      }
    }
    std::vector<std::byte> longer = bytes;
    longer.push_back(std::byte{0});
    Assert::IsFalse(Outpost::Snapshot::Read(longer, NoContent()).has_value(), L"trailing bytes are refused");
  }

  TEST_METHOD(AnAlteredByteIsRefused)
  {
    const std::vector<std::byte> bytes = Outpost::Snapshot::Write(Busy());
    for (std::size_t index = 0; index < bytes.size(); index += 7)
    {
      std::vector<std::byte> altered = bytes;
      altered[index] ^= std::byte{0x5A};
      if (Outpost::Snapshot::Read(altered, NoContent()).has_value())
      {
        Assert::Fail((L"a snapshot with byte " + std::to_wstring(index) + L" altered read back").c_str());
      }
    }
  }

  TEST_METHOD(TheHeaderIsCheckedFirst)
  {
    Neuron::ByteWriter writer;
    writer.WriteHeader({Outpost::SNAPSHOT_MAGIC, static_cast<std::uint16_t>(Outpost::SNAPSHOT_VERSION + 1)});
    Assert::IsFalse(Outpost::Snapshot::Read(writer.Bytes(), NoContent()).has_value(), L"a later version is refused");
    writer.Clear();
    writer.WriteHeader({Outpost::SNAPSHOT_MAGIC ^ 1u, Outpost::SNAPSHOT_VERSION});
    Assert::IsFalse(Outpost::Snapshot::Read(writer.Bytes(), NoContent()).has_value(), L"another magic is refused");
  }

  TEST_METHOD(ASnapshotBeforeTheFirstTickReadsBack)
  {
    const Outpost::Sim fresh(ThreeSeats(), NoContent());
    Assert::AreEqual(static_cast<std::uint32_t>(0), fresh.Tick());
    Assert::AreEqual(static_cast<std::uint64_t>(0), fresh.Hash());
    const Outpost::Sim reloaded = Reload(fresh);
    Assert::AreEqual(fresh.ComputeHash(), reloaded.ComputeHash());
  }
};

} // namespace SimTests
