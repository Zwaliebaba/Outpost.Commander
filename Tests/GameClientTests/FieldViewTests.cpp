#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
/// The seed M0 to M2 run (`GameDesign.md` section 3).
constexpr std::uint64_t MATCH_SEED = 20260922;

constexpr std::size_t QUEUE_SLOTS = 8;
constexpr std::size_t QUEUE_SLOT_BYTES = 1500;

/// A reply as `Sessions::Admit` builds it, ENCODED ONTO THE WIRE -- the client's entry point starts at the
/// bytes, so a count or a seed narrowed by the codec fails here rather than as rocks in the wrong place.
[[nodiscard]] std::vector<std::byte> EncodedReply(Outpost::JoinResult _result, std::uint64_t _seed, std::size_t _players)
{
  const bool refused = (_result == Outpost::JoinResult::MatchFull);
  const Outpost::JoinReply reply{.result = _result,
                                 .player = refused ? Outpost::NO_PLAYER : Outpost::PlayerId{1},
                                 .playerCount = refused ? std::uint8_t{0} : static_cast<std::uint8_t>(_players),
                                 .token = refused ? Outpost::NO_SESSION_TOKEN : 0x1234ull,
                                 .matchSeed = refused ? 0 : _seed};

  std::vector<std::byte> bytes(Neuron::PacketHeader::SIZE_BYTES + Outpost::JoinReply::SIZE_BYTES);
  Neuron::ByteWriter writer{bytes};
  Assert::IsTrue(Outpost::Encode(reply, writer));
  return bytes;
}

/// Delivers one encoded datagram through the frame's own drain.
void Deliver(Outpost::ClientFrame& _frame, const std::vector<std::byte>& _datagram)
{
  Neuron::PacketQueue queue{QUEUE_SLOTS, QUEUE_SLOT_BYTES};
  queue.Push(_datagram);
  static_cast<void>(_frame.DrainPackets(queue, 0));
}

[[nodiscard]] bool SameRows(std::span<const Outpost::Placement> _drawn, const std::vector<Outpost::Placement>& _generated)
{
  if (_drawn.size() != _generated.size())
  {
    return false;
  }
  for (std::size_t index = 0; index < _drawn.size(); ++index)
  {
    if (!(_drawn[index] == _generated[index]))
    {
      return false;
    }
  }
  return true;
}
} // namespace

/// M2.3. **R23 IS THAT THE TWO SIDES CANNOT DISAGREE ABOUT WHERE A ROCK IS**, and there are two ways they
/// could: a different function, or different inputs. The function is one -- `GenerateField`, in
/// `GameCore`, which the host's side and this one both compile -- so what these pin is the inputs: the
/// host's configured seed and count go into the join reply (`GameLogicTests`' `Sessions` suite), and here
/// the client takes them off the wire and derives the same rows.
///
/// **WHAT THIS CANNOT CATCH IS ONE BUILD DISAGREEING WITH ANOTHER.** x64 against ARM64 is the real claim,
/// and it rides on the four-pair determinism run in `Design/Plan/README.md`, not on anything here.
TEST_CLASS(TheClientDerivesTheField)
{
public:
  /// **THE HOST'S ROWS, FROM THE HOST'S TWO NUMBERS, THROUGH THE CLIENT'S ENTRY POINT.** Every field of
  /// every row compared, at every count a match or a stress run can have and at the seed's extremes.
  TEST_METHOD(TheFieldFromAJoinIsTheHostsField)
  {
    for (const std::size_t players : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{4}, std::size_t{8}})
    {
      for (const std::uint64_t seed : {std::uint64_t{0}, MATCH_SEED, ~std::uint64_t{0}})
      {
        Outpost::ClientFrame frame;
        Deliver(frame, EncodedReply(Outpost::JoinResult::Accepted, seed, players));

        Assert::IsTrue(frame.Field().IsDerived());
        Assert::AreEqual(seed, frame.Field().MatchSeed());
        Assert::AreEqual(players, frame.Field().PlayerCount());
        Assert::IsTrue(SameRows(frame.Field().Rocks(), Outpost::GenerateField(seed, players)), L"the client drew a different field");
      }
    }
  }

  /// **WHY THE COUNT HAS TO TRAVEL** (ADR-013 amended): one seed is two maps. A client told only the seed
  /// would have drawn one of these for the other.
  TEST_METHOD(OneSeedIsADifferentFieldAtTwoAndFourPlayers)
  {
    Outpost::ClientFrame two;
    Outpost::ClientFrame four;
    Deliver(two, EncodedReply(Outpost::JoinResult::Accepted, MATCH_SEED, 2));
    Deliver(four, EncodedReply(Outpost::JoinResult::Accepted, MATCH_SEED, 4));
    Assert::AreNotEqual(two.Field().Rocks().size(), four.Field().Rocks().size());
  }

  /// Nothing is derived before the host has answered -- there is no seed to derive from.
  TEST_METHOD(ThereIsNoFieldBeforeTheJoin)
  {
    const Outpost::ClientFrame frame;
    Assert::IsFalse(frame.Field().IsDerived());
    Assert::AreEqual(std::size_t{0}, frame.Field().Rocks().size());
  }

  /// A refused client is in no match and draws no rocks.
  TEST_METHOD(ARefusalDerivesNothing)
  {
    Outpost::ClientFrame frame;
    Deliver(frame, EncodedReply(Outpost::JoinResult::MatchFull, MATCH_SEED, 2));
    Assert::IsFalse(frame.Field().IsDerived());
    Assert::AreEqual(std::size_t{0}, frame.Field().Rocks().size());
  }

  /// **A REJOIN INTO THE SAME MATCH KEEPS THE SAME FIELD**, and a late refusal -- a reply in flight before
  /// the seat existed, which `JoinState` ignores -- does not clear it.
  TEST_METHOD(ARejoinAndALateRefusalLeaveTheFieldAlone)
  {
    Outpost::ClientFrame frame;
    Deliver(frame, EncodedReply(Outpost::JoinResult::Accepted, MATCH_SEED, 2));
    const std::vector<Outpost::Placement> before{frame.Field().Rocks().begin(), frame.Field().Rocks().end()};

    Deliver(frame, EncodedReply(Outpost::JoinResult::Rejoined, MATCH_SEED, 2));
    Assert::IsTrue(SameRows(frame.Field().Rocks(), before));

    Deliver(frame, EncodedReply(Outpost::JoinResult::MatchFull, MATCH_SEED, 2));
    Assert::IsTrue(frame.Field().IsDerived(), L"a late refusal unseated the field");
    Assert::IsTrue(SameRows(frame.Field().Rocks(), before));
  }
};

/// The class on its own, without a frame around it.
TEST_CLASS(TheFieldView)
{
public:
  /// **A REPEAT OF THE SAME PAIR IS NOT A CHANGE**, since the host answers every retry and each answer
  /// passes through `Derive`; a new seed or a new count is.
  TEST_METHOD(OnlyANewPairRederives)
  {
    Outpost::FieldView view;
    Assert::IsTrue(view.Derive(MATCH_SEED, 2));
    Assert::IsFalse(view.Derive(MATCH_SEED, 2));
    Assert::IsTrue(view.Derive(MATCH_SEED + 1, 2));
    Assert::IsTrue(view.Derive(MATCH_SEED + 1, 4));
    Assert::IsTrue(SameRows(view.Rocks(), Outpost::GenerateField(MATCH_SEED + 1, 4)));
  }

  /// A count of zero is no match: it clears, and says so only when there was something to clear.
  TEST_METHOD(ACountOfZeroClears)
  {
    Outpost::FieldView view;
    Assert::IsFalse(view.Derive(MATCH_SEED, 0));
    Assert::IsTrue(view.Derive(MATCH_SEED, 2));
    Assert::IsTrue(view.Derive(MATCH_SEED, 0));
    Assert::IsFalse(view.IsDerived());
    Assert::AreEqual(std::size_t{0}, view.Rocks().size());
  }
};

} // namespace GameClientTests
