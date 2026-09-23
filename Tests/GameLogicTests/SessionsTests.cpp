#include "pch.h"

#include <set>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
[[nodiscard]] Neuron::Endpoint At(std::uint32_t _address, std::uint16_t _port) noexcept
{
  return Neuron::Endpoint{.addressV4 = _address, .port = _port};
}

constexpr std::uint64_t SEED = 20260922;
} // namespace

/// ADR-013's slot table. **Every one of these is a socket-free test of something that used to need
/// two machines**, which is the whole reason `Sessions` holds no transport.
TEST_CLASS(SeatingAClient)
{
public:
  TEST_METHOD(TheFirstClientIsPlayerOne)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, SEED);

    const Outpost::JoinReply reply = sessions.Admit(Outpost::Join{}, At(1, 100));
    Assert::IsTrue(reply.result == Outpost::JoinResult::Accepted);
    Assert::AreEqual(1, static_cast<int>(reply.player));
    Assert::AreEqual(SEED, reply.matchSeed);
    Assert::IsTrue(reply.token != Outpost::NO_SESSION_TOKEN, L"zero is what a client sends to say it has none");
  }

  /// **THE HOST ASSIGNS AND THE CLIENT DOES NOT CHOOSE** (ADR-013), lowest slot first -- so a solo
  /// client is always player one and a suite does not have to care about arrival order.
  TEST_METHOD(SlotsAreHandedOutLowestFirst)
  {
    Outpost::Sessions sessions;
    sessions.Begin(4, SEED);

    Assert::AreEqual(1, static_cast<int>(sessions.Admit(Outpost::Join{}, At(1, 100)).player));
    Assert::AreEqual(2, static_cast<int>(sessions.Admit(Outpost::Join{}, At(1, 101)).player));
    Assert::AreEqual(3, static_cast<int>(sessions.Admit(Outpost::Join{}, At(2, 100)).player));
    Assert::AreEqual(4, static_cast<int>(sessions.Admit(Outpost::Join{}, At(2, 101)).player));
    Assert::AreEqual(static_cast<std::size_t>(4), sessions.Count());
  }

  /// **A REFUSAL IS A REPLY AND NOT A SILENCE.** A client that is dropped cannot tell a full match
  /// from a host that is not running, and `GameDesign.md` section 2 holds a slot indefinitely, so
  /// this does not become false by waiting.
  TEST_METHOD(AMatchWithNoFreeSlotRefusesWithAReason)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, SEED);
    static_cast<void>(sessions.Admit(Outpost::Join{}, At(1, 100)));
    static_cast<void>(sessions.Admit(Outpost::Join{}, At(1, 101)));

    const Outpost::JoinReply refused = sessions.Admit(Outpost::Join{}, At(9, 999));
    Assert::IsTrue(refused.result == Outpost::JoinResult::MatchFull);
    Assert::AreEqual(static_cast<int>(Outpost::NO_PLAYER), static_cast<int>(refused.player));
    Assert::AreEqual(Outpost::NO_SESSION_TOKEN, refused.token);

    // The seed is not handed to somebody who is not in the match.
    Assert::AreEqual(static_cast<std::uint64_t>(0), refused.matchSeed);
    Assert::AreEqual(static_cast<std::size_t>(2), sessions.Count(), L"a refusal must not have taken a seat");
  }

  /// **THE POINT OF THE WHOLE RECORD.** A client's ephemeral port does not survive a relaunch, so a
  /// reconnect arrives from an endpoint nothing has ever seen -- and gets its slot back because of
  /// the token and nothing else.
  TEST_METHOD(AReconnectFromANewEndpointKeepsItsSlot)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, SEED);

    static_cast<void>(sessions.Admit(Outpost::Join{}, At(1, 100)));
    const Outpost::JoinReply seated = sessions.Admit(Outpost::Join{}, At(1, 101));
    Assert::AreEqual(2, static_cast<int>(seated.player));

    const Neuron::Endpoint returning = At(1, 54321);
    const Outpost::JoinReply back = sessions.Admit(Outpost::Join{.token = seated.token}, returning);

    Assert::IsTrue(back.result == Outpost::JoinResult::Rejoined);
    Assert::AreEqual(static_cast<int>(seated.player), static_cast<int>(back.player));
    Assert::AreEqual(seated.token, back.token);
    Assert::AreEqual(SEED, back.matchSeed, L"a reconnecting client must redraw the same field");

    // The seat moved rather than multiplied, and the host now sends there.
    Assert::AreEqual(static_cast<std::size_t>(2), sessions.Count());
    Assert::AreEqual(2, static_cast<int>(sessions.PlayerAt(returning)));
    Assert::AreEqual(static_cast<int>(Outpost::NO_PLAYER), static_cast<int>(sessions.PlayerAt(At(1, 101))));
  }

  /// **THE LOST-REPLY WINDOW, AND IT IS THE ONE HAZARD THE TOKEN MODEL CARRIES.** The host issued a
  /// token and the reply never arrived, so the client retries with nothing. Without the endpoint
  /// match it would be given a SECOND slot while the first is held indefinitely -- at two slots, a
  /// match full with one player in it.
  TEST_METHOD(ARetryFromTheSameEndpointGetsTheSameSeat)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, SEED);

    const Neuron::Endpoint client = At(1, 100);
    const Outpost::JoinReply first = sessions.Admit(Outpost::Join{}, client);
    const Outpost::JoinReply retry = sessions.Admit(Outpost::Join{}, client);

    Assert::IsTrue(retry.result == Outpost::JoinResult::Accepted);
    Assert::AreEqual(static_cast<int>(first.player), static_cast<int>(retry.player));
    Assert::AreEqual(first.token, retry.token);
    Assert::AreEqual(static_cast<std::size_t>(1), sessions.Count(), L"a retry took a second slot");
  }

  /// A token from a host that is no longer running, presented from the same live socket. It is the
  /// same client and it gets the seat it already has, rather than a second one.
  TEST_METHOD(AStaleTokenFromASeatedEndpointIsNotASecondSeat)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, SEED);

    const Neuron::Endpoint client = At(1, 100);
    const Outpost::JoinReply first = sessions.Admit(Outpost::Join{}, client);
    const Outpost::JoinReply again = sessions.Admit(Outpost::Join{.token = 0xDEADBEEFDEADBEEFull}, client);

    Assert::AreEqual(first.token, again.token);
    Assert::AreEqual(static_cast<std::size_t>(1), sessions.Count());
  }

  /// A token this host never issued, from an endpoint it has never seen, is a new client.
  TEST_METHOD(AnUnknownTokenFromAnUnknownEndpointIsANewClient)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, SEED);

    const Outpost::JoinReply reply = sessions.Admit(Outpost::Join{.token = 0x1111111111111111ull}, At(7, 7));
    Assert::IsTrue(reply.result == Outpost::JoinResult::Accepted);
    Assert::AreEqual(1, static_cast<int>(reply.player));
    Assert::IsTrue(reply.token != 0x1111111111111111ull, L"a client does not choose its own identity");
  }

  /// **THE HOST TRUSTS THIS AND NOT THE PLAYER BYTE ON A COMMAND PACKET** (ADR-013).
  TEST_METHOD(AnEndpointWithNoSessionIsNobody)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, SEED);
    static_cast<void>(sessions.Admit(Outpost::Join{}, At(1, 100)));

    Assert::AreEqual(1, static_cast<int>(sessions.PlayerAt(At(1, 100))));
    Assert::AreEqual(static_cast<int>(Outpost::NO_PLAYER), static_cast<int>(sessions.PlayerAt(At(1, 101))));
    Assert::AreEqual(static_cast<int>(Outpost::NO_PLAYER), static_cast<int>(sessions.PlayerAt(Neuron::Endpoint{})));
  }
};

/// The token itself, which ADR-013 calls a name rather than a credential -- and which still has to
/// be a name nobody else answers to.
TEST_CLASS(SessionTokens)
{
public:
  /// **ZERO IS NEVER ISSUED**, which is what lets `NO_SESSION_TOKEN` mean "I have none" with no
  /// flag byte beside it.
  TEST_METHOD(NoIssuedTokenIsZeroAndNoneRepeats)
  {
    std::set<Outpost::SessionToken> issued;
    for (std::uint64_t seed = 0; seed < 40; ++seed)
    {
      Outpost::Sessions sessions;
      sessions.Begin(4, seed);
      for (std::uint16_t client = 0; client < 4; ++client)
      {
        const Outpost::JoinReply reply = sessions.Admit(Outpost::Join{}, At(1, client));
        Assert::IsTrue(reply.token != Outpost::NO_SESSION_TOKEN);
        Assert::IsTrue(issued.insert(reply.token).second, L"two clients were given one name");
      }
    }
    Assert::AreEqual(static_cast<std::size_t>(160), issued.size());
  }

  /// **SIXTY-FOUR BITS OUT OF TWO THIRTY-TWO-BIT DRAWS, IN A FIXED ORDER.** Written as one
  /// expression the two calls are unsequenced and the halves land wherever the compiler puts them,
  /// so two builds issue different tokens from one seed -- which is R16's defect exactly, and is
  /// invisible to a suite that runs one build. This asserts both halves carry entropy.
  TEST_METHOD(ATokenIsWiderThanThirtyTwoBits)
  {
    Outpost::Sessions sessions;
    sessions.Begin(4, SEED);

    std::uint64_t high = 0;
    std::uint64_t low = 0;
    for (std::uint16_t client = 0; client < 4; ++client)
    {
      const Outpost::SessionToken token = sessions.Admit(Outpost::Join{}, At(1, client)).token;
      high |= (token >> 32);
      low |= (token & 0xFFFFFFFFull);
    }
    Assert::IsTrue(high != 0, L"the top half of every token was zero");
    Assert::IsTrue(low != 0, L"the bottom half of every token was zero");
  }

  /// **DETERMINISTIC FROM THE SEED**, which is what lets a suite pin a reconnect and is why the
  /// generator is `Neuron::Pcg32` on its own stream rather than anything that reads a clock (R16).
  /// ADR-013 names the consequence: a client holding the seed can compute the tokens, and section 5
  /// declines authentication outright, so a token is a name and not a credential.
  TEST_METHOD(TwoRunsOfOneMatchIssueTheSameTokens)
  {
    Outpost::Sessions first;
    Outpost::Sessions second;
    first.Begin(4, SEED);
    second.Begin(4, SEED);

    for (std::uint16_t client = 0; client < 4; ++client)
    {
      Assert::AreEqual(first.Admit(Outpost::Join{}, At(1, client)).token, second.Admit(Outpost::Join{}, At(5, client)).token);
    }
  }

  /// A different seed is a different match and different names.
  TEST_METHOD(ADifferentSeedIssuesDifferentTokens)
  {
    Outpost::Sessions first;
    Outpost::Sessions second;
    first.Begin(4, SEED);
    second.Begin(4, SEED + 1);

    Assert::IsTrue(first.Admit(Outpost::Join{}, At(1, 1)).token != second.Admit(Outpost::Join{}, At(1, 1)).token);
  }

  /// `Begin` starts a match rather than continuing one: the seats go and the seed changes, which is
  /// what `Interface.md` section 7 has the host do at victory.
  TEST_METHOD(BeginningAMatchForgetsEverySeat)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, SEED);
    const Outpost::JoinReply seated = sessions.Admit(Outpost::Join{}, At(1, 100));

    sessions.Begin(2, SEED + 5);
    Assert::AreEqual(static_cast<std::size_t>(0), sessions.Count());
    Assert::AreEqual(SEED + 5, sessions.MatchSeed());

    // The old token names nobody, so the client is seated as new -- with the new seed, which is
    // the field it now has to draw.
    const Outpost::JoinReply after = sessions.Admit(Outpost::Join{.token = seated.token}, At(1, 100));
    Assert::IsTrue(after.result == Outpost::JoinResult::Accepted);
    Assert::AreEqual(SEED + 5, after.matchSeed);
  }

  /// A player count above the capacity is clamped rather than refused, because `PlayerCountAllowed`
  /// refuses it before any caller gets here and there is nothing useful to return. **A stress count below
  /// it is kept** (ADR-023): ninety-nine is a count a host can seat.
  TEST_METHOD(ThePlayerCountIsClampedToTheCapacity)
  {
    Outpost::Sessions sessions;
    sessions.Begin(99, SEED);
    Assert::AreEqual(std::size_t{99}, sessions.PlayerCount());

    sessions.Begin(300, SEED);
    Assert::AreEqual(Outpost::Sessions::MAX_PLAYERS, sessions.PlayerCount());
    Assert::AreEqual(std::size_t{254}, Outpost::Sessions::MAX_PLAYERS);
  }

  /// A match with no slots seats nobody, which is the honest answer rather than a crash.
  TEST_METHOD(AMatchWithNoSlotsRefusesEverybody)
  {
    Outpost::Sessions sessions;
    sessions.Begin(0, SEED);
    Assert::IsTrue(sessions.Admit(Outpost::Join{}, At(1, 1)).result == Outpost::JoinResult::MatchFull);
  }
};

} // namespace GameLogicTests
