#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr std::uint64_t INTERVAL = Outpost::JoinState::RETRY_INTERVAL_MILLISECONDS;

/// A four-player match, so every slot these tests seat is one the count allows.
constexpr std::uint8_t PLAYERS = 4;

/// What the host sends: the seed and the count on a seat, and neither on a refusal (`Sessions::Admit`).
[[nodiscard]] Outpost::JoinReply Seated(Outpost::JoinResult _result, Outpost::PlayerId _player, Outpost::SessionToken _token)
{
  const bool refused = (_result == Outpost::JoinResult::MatchFull);
  return Outpost::JoinReply{.result = _result,
                            .player = _player,
                            .playerCount = refused ? std::uint8_t{0} : PLAYERS,
                            .token = _token,
                            .matchSeed = refused ? 0 : 0xFEEDFACEull};
}
} // namespace

/// ADR-013's handshake from the client's side, and **every one of these runs without a socket** --
/// which is the same split M0.18 forced on the gesture seam and M0.19 on the playout clock.
TEST_CLASS(TheJoinRetry)
{
public:
  /// **THE FIRST ONE GOES OUT AT ONCE.** There is no reason to look at a reconnecting overlay for a
  /// quarter second before the client has even asked.
  TEST_METHOD(TheFirstJoinIsSentImmediately)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);
    Assert::IsTrue(join.ShouldSend(0));
    Assert::AreEqual(1u, join.SentCount());
  }

  TEST_METHOD(ARetryWaitsTheIntervalAndThenGoes)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);
    Assert::IsTrue(join.ShouldSend(1000));

    Assert::IsFalse(join.ShouldSend(1000));
    Assert::IsFalse(join.ShouldSend(1000 + INTERVAL - 1));
    Assert::IsTrue(join.ShouldSend(1000 + INTERVAL));
    Assert::AreEqual(2u, join.SentCount());
  }

  /// **THERE IS NO TIMEOUT AND THAT IS DELIBERATE.** The common failure is a host that has not been
  /// started yet, and `GameDesign.md` section 2 holds the slot indefinitely on the other side, so
  /// nothing expires.
  TEST_METHOD(ItKeepsAskingForever)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);

    std::uint64_t now = 0;
    for (std::uint32_t attempt = 0; attempt < 2000; ++attempt)
    {
      Assert::IsTrue(join.ShouldSend(now));
      now += INTERVAL;
    }
    Assert::AreEqual(2000u, join.SentCount());
    Assert::IsTrue(join.Phase() == Outpost::JoinPhase::Joining);
  }

  /// A seated client says nothing more. The host answers every join it receives, so a client that
  /// kept asking would be answered forever.
  TEST_METHOD(ASeatedClientStopsAsking)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);
    Assert::IsTrue(join.ShouldSend(0));

    static_cast<void>(join.Accept(Seated(Outpost::JoinResult::Accepted, 1, 77)));
    Assert::IsFalse(join.ShouldSend(10 * INTERVAL));
    Assert::AreEqual(1u, join.SentCount());
  }

  /// **A REFUSAL IS TERMINAL.** `MatchFull` does not become false by waiting, because the slot it
  /// wanted is held indefinitely.
  TEST_METHOD(ARefusedClientStopsAsking)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);
    Assert::IsTrue(join.ShouldSend(0));

    static_cast<void>(join.Accept(Seated(Outpost::JoinResult::MatchFull, Outpost::NO_PLAYER, 0)));
    Assert::IsTrue(join.Phase() == Outpost::JoinPhase::Refused);
    Assert::IsFalse(join.ShouldSend(100 * INTERVAL));
  }

  /// The token from `LocalState` is what goes out, so a returning client is recognized.
  TEST_METHOD(TheStoredTokenIsWhatIsSent)
  {
    Outpost::JoinState join;
    join.Begin(0x1234567890ABCDEFull);
    Assert::AreEqual(0x1234567890ABCDEFull, join.Outgoing().token);
  }
};

TEST_CLASS(TheJoinReply)
{
public:
  /// The seed AND the count (M2.3): the field is derived from the pair, and the seed alone is two maps.
  TEST_METHOD(AnAcceptanceSeatsTheClientAndCarriesTheSeedAndTheCount)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);
    Assert::AreEqual(std::size_t{0}, join.PlayerCount(), L"no count before the host has said");

    Assert::IsTrue(join.Accept(Seated(Outpost::JoinResult::Accepted, 2, 0xAAAAull)), L"a new token has to be persisted");
    Assert::IsTrue(join.IsJoined());
    Assert::AreEqual(2, static_cast<int>(join.Player()));
    Assert::AreEqual(0xAAAAull, join.Token());
    Assert::AreEqual(0xFEEDFACEull, join.MatchSeed());
    Assert::AreEqual(std::size_t{PLAYERS}, join.PlayerCount());
    Assert::IsFalse(join.Resumed());
  }

  /// `Rejoined` is kept because it is the difference between resuming a match and starting one,
  /// which is what `Interface.md` section 7's overlay is about.
  TEST_METHOD(ARejoinIsDistinguishableFromAFreshSeat)
  {
    Outpost::JoinState join;
    join.Begin(0xAAAAull);

    Assert::IsFalse(join.Accept(Seated(Outpost::JoinResult::Rejoined, 2, 0xAAAAull)), L"an unchanged token is not worth a write");
    Assert::IsTrue(join.IsJoined());
    Assert::IsTrue(join.Resumed());
    Assert::AreEqual(2, static_cast<int>(join.Player()));
  }

  /// **THE HOST ANSWERS EVERY RETRY, SO DUPLICATES ARE ORDINARY.** A second copy of the same reply
  /// must not look like a change, or the client writes the same token to `LocalState` on every
  /// frame a stale reply arrives.
  TEST_METHOD(ADuplicateReplyChangesNothing)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);

    const Outpost::JoinReply reply = Seated(Outpost::JoinResult::Accepted, 1, 0xBBBBull);
    Assert::IsTrue(join.Accept(reply));
    Assert::IsFalse(join.Accept(reply));
    Assert::IsFalse(join.Accept(reply));
    Assert::AreEqual(0xBBBBull, join.Token());
  }

  /// **A SEATED CLIENT IS NOT UNSEATED BY A LATE REFUSAL.** The only way to see one after being
  /// seated is a reply that was in flight before the seat existed, and acting on it throws away a
  /// slot the host still holds.
  TEST_METHOD(ALateRefusalDoesNotUnseatAClient)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);
    static_cast<void>(join.Accept(Seated(Outpost::JoinResult::Accepted, 1, 0xCCCCull)));

    Assert::IsFalse(join.Accept(Seated(Outpost::JoinResult::MatchFull, Outpost::NO_PLAYER, 0)));
    Assert::IsTrue(join.IsJoined());
    Assert::AreEqual(1, static_cast<int>(join.Player()));
    Assert::AreEqual(0xCCCCull, join.Token());
  }

  /// A refusal clears the token, so "I was refused" and "I have never joined" are one state on the
  /// next run rather than two.
  TEST_METHOD(ARefusalClearsTheTokenAndSaysSo)
  {
    Outpost::JoinState join;
    join.Begin(0xDDDDull);

    Assert::IsTrue(join.Accept(Seated(Outpost::JoinResult::MatchFull, Outpost::NO_PLAYER, 0)), L"the cleared token has to be persisted");
    Assert::AreEqual(Outpost::NO_SESSION_TOKEN, join.Token());
    Assert::AreEqual(static_cast<int>(Outpost::NO_PLAYER), static_cast<int>(join.Player()));
    Assert::AreEqual(std::size_t{0}, join.PlayerCount(), L"a refused client is in no match and has no field");

    // And a client that had none to begin with has nothing to write.
    Outpost::JoinState fresh;
    fresh.Begin(Outpost::NO_SESSION_TOKEN);
    Assert::IsFalse(fresh.Accept(Seated(Outpost::JoinResult::MatchFull, Outpost::NO_PLAYER, 0)));
  }

  /// `Begin` starts over, which is what a relaunch is.
  TEST_METHOD(BeginningAgainForgetsTheSeat)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);
    static_cast<void>(join.Accept(Seated(Outpost::JoinResult::Accepted, 3, 0xEEEEull)));

    join.Begin(0xEEEEull);
    Assert::IsFalse(join.IsJoined());
    Assert::AreEqual(static_cast<int>(Outpost::NO_PLAYER), static_cast<int>(join.Player()));
    Assert::AreEqual(static_cast<std::uint64_t>(0), join.MatchSeed());
    Assert::AreEqual(std::size_t{0}, join.PlayerCount());
    Assert::IsTrue(join.ShouldSend(0));
  }
};

/// `Interface.md` section 7's reconnect: a seated client asks again **as itself**.
TEST_CLASS(TheRejoin)
{
public:
  /// The token goes out again, so the host answers `Rejoined` into the same slot -- and the player and
  /// the seed are kept, so the panels behind the overlay still read this player's block.
  TEST_METHOD(ARejoinAsksAgainWithTheSameTokenAndKeepsTheSeat)
  {
    Outpost::JoinState join;
    join.Begin(Outpost::NO_SESSION_TOKEN);
    Assert::IsTrue(join.ShouldSend(0));
    static_cast<void>(join.Accept(Seated(Outpost::JoinResult::Accepted, 2, 0x5151ull)));
    Assert::IsFalse(join.ShouldSend(10 * INTERVAL));

    join.Rejoin();
    Assert::IsTrue(join.Phase() == Outpost::JoinPhase::Joining);
    Assert::IsTrue(join.ShouldSend(10 * INTERVAL), L"the first join after a loss goes out at once");
    Assert::AreEqual(0x5151ull, join.Outgoing().token);
    Assert::AreEqual(2, static_cast<int>(join.Player()));
    Assert::AreEqual(0xFEEDFACEull, join.MatchSeed());

    Assert::IsFalse(join.Accept(Seated(Outpost::JoinResult::Rejoined, 2, 0x5151ull)));
    Assert::IsTrue(join.IsJoined());
    Assert::IsTrue(join.Resumed());
  }

  /// **A REFUSAL IS NOT UNDONE BY A LOSS**, and a client still asking is already doing what a rejoin
  /// would make it do.
  TEST_METHOD(OnlyASeatedClientRejoins)
  {
    Outpost::JoinState refused;
    refused.Begin(Outpost::NO_SESSION_TOKEN);
    static_cast<void>(refused.Accept(Seated(Outpost::JoinResult::MatchFull, Outpost::NO_PLAYER, 0)));
    refused.Rejoin();
    Assert::IsTrue(refused.Phase() == Outpost::JoinPhase::Refused);
    Assert::IsFalse(refused.ShouldSend(100 * INTERVAL));

    Outpost::JoinState joining;
    joining.Begin(Outpost::NO_SESSION_TOKEN);
    Assert::IsTrue(joining.ShouldSend(0));
    joining.Rejoin();
    Assert::IsFalse(joining.ShouldSend(1), L"the retry cadence is not reset by a rejoin that did nothing");
  }
};

} // namespace GameClientTests
