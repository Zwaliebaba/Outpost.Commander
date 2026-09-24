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

  /// **THE HOST'S HALF OF R23** (M2.3): every seat carries the seed and the CONFIGURED count -- not how
  /// many have joined -- so the first client of a four-player match derives the four-player field. A
  /// rejoin carries the same pair and a refusal carries neither.
  TEST_METHOD(EverySeatCarriesTheConfiguredCountAndARefusalDoesNot)
  {
    for (const std::size_t players : {std::size_t{1}, std::size_t{2}, std::size_t{4}, std::size_t{8}})
    {
      Outpost::Sessions sessions;
      sessions.Begin(players, SEED);

      Outpost::JoinReply first{};
      for (std::size_t client = 0; client < players; ++client)
      {
        const Outpost::JoinReply reply = sessions.Admit(Outpost::Join{}, At(1, static_cast<std::uint16_t>(100 + client)));
        Assert::IsTrue(reply.result == Outpost::JoinResult::Accepted);
        Assert::AreEqual(players, static_cast<std::size_t>(reply.playerCount));
        Assert::AreEqual(SEED, reply.matchSeed);
        if (client == 0)
        {
          first = reply;
        }
      }

      const Outpost::JoinReply back = sessions.Admit(Outpost::Join{.token = first.token}, At(2, 200));
      Assert::IsTrue(back.result == Outpost::JoinResult::Rejoined);
      Assert::AreEqual(players, static_cast<std::size_t>(back.playerCount));

      const Outpost::JoinReply refused = sessions.Admit(Outpost::Join{}, At(9, 999));
      Assert::IsTrue(refused.result == Outpost::JoinResult::MatchFull);
      Assert::AreEqual(0, static_cast<int>(refused.playerCount));
    }
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
    for (std::uint64_t salt = 0; salt < 40; ++salt)
    {
      Outpost::Sessions sessions;
      sessions.SaltTokens(salt);
      sessions.Begin(4, SEED);
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

  /// **DETERMINISTIC FROM THE SALT**, which is what lets a suite pin a reconnect and is why the
  /// generator is `Neuron::Pcg32` on its own stream rather than anything that reads a clock (R16). The
  /// clock is read once, by `Server`, and handed in (`Sessions::SaltTokens`). ADR-013 names the
  /// consequence: a token is a name and not a credential.
  TEST_METHOD(TwoTablesWithOneSaltIssueTheSameTokens)
  {
    Outpost::Sessions first;
    Outpost::Sessions second;
    first.SaltTokens(77);
    second.SaltTokens(77);
    first.Begin(4, SEED);
    second.Begin(4, SEED + 1);

    for (std::uint16_t client = 0; client < 4; ++client)
    {
      Assert::AreEqual(first.Admit(Outpost::Join{}, At(1, client)).token, second.Admit(Outpost::Join{}, At(5, client)).token);
    }
  }

  /// A different salt is a different host run and different names, whatever the seed.
  TEST_METHOD(ADifferentSaltIssuesDifferentTokens)
  {
    Outpost::Sessions first;
    Outpost::Sessions second;
    first.SaltTokens(1);
    second.SaltTokens(2);
    first.Begin(4, SEED);
    second.Begin(4, SEED);

    Assert::IsTrue(first.Admit(Outpost::Join{}, At(1, 1)).token != second.Admit(Outpost::Join{}, At(1, 1)).token);
  }

  /// **THE REVIEW'S B4, AS IT HAPPENED** (2026-09-23). Evening one seats A as player 1 and B as player 2.
  /// The host is restarted on the same seed and B launches first. When the tokens were a function of the
  /// seed, B was seated as player 1 and issued A's old token, A then rejoined seat 1 with it, and the two
  /// evicted each other every second with seat 2 never taken. With a new salt B's seat carries a new name,
  /// A's old token names nobody, and A takes seat 2.
  TEST_METHOD(AHostRestartedOnTheSameSeedDoesNotReissueLastRunsTokens)
  {
    Outpost::Sessions evening;
    evening.SaltTokens(1000);
    evening.Begin(2, SEED);
    const Outpost::JoinReply a = evening.Admit(Outpost::Join{}, At(1, 100));
    const Outpost::JoinReply b = evening.Admit(Outpost::Join{}, At(2, 200));

    Outpost::Sessions restarted;
    restarted.SaltTokens(2000);
    restarted.Begin(2, SEED);
    const Outpost::JoinReply bAgain = restarted.Admit(Outpost::Join{.token = b.token}, At(2, 201));
    Assert::AreEqual(1, static_cast<int>(bAgain.player));
    Assert::IsTrue(bAgain.token != a.token, L"the restarted host handed out last run's token for seat 1");

    const Outpost::JoinReply aAgain = restarted.Admit(Outpost::Join{.token = a.token}, At(1, 101));
    Assert::IsTrue(aAgain.result == Outpost::JoinResult::Accepted, L"an old token must name nobody");
    Assert::AreEqual(2, static_cast<int>(aAgain.player));
    Assert::AreEqual(1, static_cast<int>(restarted.PlayerAt(At(2, 201))), L"B lost its seat");
  }

  /// **A LATER MATCH ON ONE TABLE CONTINUES THE STREAM** rather than repeating it, so the same collision
  /// cannot happen at a restart inside one host run either: no token a previous `Begin` issued names a seat
  /// of this one.
  TEST_METHOD(ATokenFromAPreviousMatchNeverNamesASeatOfThisOne)
  {
    Outpost::Sessions sessions;
    sessions.SaltTokens(5);
    std::set<Outpost::SessionToken> earlier;
    for (int match = 0; match < 10; ++match)
    {
      sessions.Begin(4, SEED);
      for (std::uint16_t client = 0; client < 4; ++client)
      {
        const Outpost::SessionToken token = sessions.Admit(Outpost::Join{}, At(1, client)).token;
        Assert::IsTrue(earlier.insert(token).second, L"a restart on the same seed reissued a token");
      }
    }
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

/// M3.8, `OpenQuestions.md` Q70. **A restart keeps every seat.**
TEST_CLASS(TheNextMatch)
{
public:
  /// A client that joins again with its token after a reseed takes the seat it had, told the new seed.
  TEST_METHOD(AReseedKeepsTheSeatAndTellsTheNewSeed)
  {
    Outpost::Sessions sessions;
    sessions.Begin(2, SEED);
    const Outpost::JoinReply first = sessions.Admit(Outpost::Join{}, At(1, 100));
    const Outpost::JoinReply second = sessions.Admit(Outpost::Join{}, At(1, 101));

    sessions.Reseed(SEED + 1);
    Assert::AreEqual(std::size_t{2}, sessions.Count(), L"a restart dropped a seat");

    // The other way round, so arrival order cannot be what gives each its seat back.
    const Outpost::JoinReply secondBack = sessions.Admit(Outpost::Join{.token = second.token}, At(1, 101));
    const Outpost::JoinReply firstBack = sessions.Admit(Outpost::Join{.token = first.token}, At(1, 100));
    Assert::AreEqual(first.player, firstBack.player);
    Assert::AreEqual(second.player, secondBack.player);
    Assert::AreEqual(SEED + 1, firstBack.matchSeed);
    Assert::IsTrue(firstBack.result == Outpost::JoinResult::Rejoined);
  }
};

} // namespace GameLogicTests
