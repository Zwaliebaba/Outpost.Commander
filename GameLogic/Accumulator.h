#pragma once

#include "World.h"

#include "GameCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// One entity as a client is told about it. **The one place the lossy step of replication happens**:
/// positions, heading and hull are quantized here and nowhere else on the host.
[[nodiscard]] EntityRecord RecordOf(const Entity& _entity) noexcept;

/// ADR-024's priority accumulator: which entity records each client is sent, tick by tick.
///
/// **EVERY CLIENT HAS A SCORE PER LIVE ENTITY.** Each tick the score grows by that entity's relevance to
/// that client, sending the entity resets it, and the host fills the client's updates with the highest
/// scores that fit. Relevance decides how OFTEN an entity is refreshed. **The sweep decides how LONG it
/// can go without**: an entity unsent for `SweepTicks` goes ahead of every score, whatever they are, so
/// no weighting can starve one.
///
/// **THIS IS PER-CLIENT HOST STATE, NOT SIMULATION.** It reads the world after the tick and writes
/// nothing the tick reads or the state hash covers, so M1.7's determinism test cannot see it and must
/// come out unchanged. It is still written the way R16 asks -- integers, a total order on every sort,
/// clients walked in the order they were first seen -- because a host that sends two different sets
/// from the same state is a host whose suite cannot pin what a client saw.
///
/// **IT IS PURE AND HOLDS NO SOCKET**, the split every seam here takes: `Host` hands it a world and
/// sends what comes back.
class Accumulator
{
public:
  /// **THE RELEVANCE WEIGHTS, AND THEY ARE `OpenQuestions.md` Q49's TO TUNE.** Built to its
  /// recommendation. An entity that is in view and moved scores 1 + 4 + 2 = 7 a tick; one out of view and
  /// still scores 1 -- a seven-to-one ratio, which is roughly what keeps a thousand ships in view
  /// refreshing within six ticks at the cap while everything else waits for the sweep.
  ///
  /// The BASE is what makes the accumulator an accumulator: without it an entity that is out of view and
  /// idle would never rise, and only the sweep would ever send it.
  static constexpr std::uint64_t WEIGHT_BASE = 1;
  /// Inside the client's view. The term a player feels most directly, so the largest.
  static constexpr std::uint64_t WEIGHT_IN_VIEW = 4;
  /// Its record differs from the last one this client was sent -- it moved, turned, took damage.
  static constexpr std::uint64_t WEIGHT_CHANGED = 2;
  /// The client's own. A player watches their own fleet more closely than anyone else's.
  static constexpr std::uint64_t WEIGHT_OWN = 2;

  /// Forgets every client. What starting a match means.
  void Begin() noexcept;

  /// **A REJOIN: EVERYTHING DUE AT ONCE.** The client clears its store when it rejoins, so every entity
  /// is sent within one sweep, and the removals it missed are dropped rather than repeated -- it has
  /// nothing to remove them from. The view and the sequence are kept.
  void Reset(PlayerId _player) noexcept;

  /// What the client reported it is looking at, from the command packet's header. A radius of zero is
  /// no view, and nothing then scores as in view.
  void SetView(PlayerId _player, std::int16_t _wireX, std::int16_t _wireY, std::uint16_t _radiusUnits) noexcept;

  /// A shot, to ride every client's next `FIRE_REPEAT_TICKS` updates. Nothing calls this before M3.
  void NoteFire(const FireEvent& _fire);

  /// The updates this client is due this tick: **at least one**, so a client hears its own block and the
  /// tick every tick whatever else is due, and at most `UPDATES_PER_TICK`, each of them whole.
  ///
  /// It is where deaths become removals: an entity this client was sent that is no longer in its slot,
  /// or whose slot now holds somebody else, is queued to ride the next `REMOVAL_REPEAT_TICKS` updates.
  [[nodiscard]] std::vector<Update> Fill(const World& _world, PlayerId _player, const PlayerBlock& _own, std::uint32_t _tick);

  /// Clients this has seen. For a suite, and for the harness's report.
  [[nodiscard]] std::size_t ClientCount() const noexcept
  {
    return m_clients.size();
  }

private:
  /// One entity as one client has been told about it.
  struct Tracked
  {
    std::uint64_t score = 0;
    std::uint32_t lastSentTick = 0;
    bool sent = false;

    /// The record last sent, which is what "changed" is judged against and what names a removal.
    EntityRecord lastSent{};
  };

  struct PendingRemoval
  {
    WireIdentity identity = NO_WIRE_IDENTITY;
    std::uint32_t remaining = 0;
  };

  struct PendingFire
  {
    FireEvent event{};
    std::uint32_t remaining = 0;
  };

  struct Client
  {
    PlayerId player = NO_PLAYER;

    /// Indexed by the store's slot, and grown to the store's slot count on every fill -- a slot, once
    /// allocated, is reused rather than removed (`World`), so this never has to shrink.
    std::vector<Tracked> tracked;

    std::vector<PendingRemoval> removals;
    std::vector<PendingFire> fires;

    /// The view, in the simulation's own units, and whether there is one.
    Neuron::Vec2 viewCenter{};
    std::int64_t viewRadiusUnits = 0;

    /// This client's transport sequence. Per recipient, so a client can count its own loss.
    std::uint16_t sequence = 0;
  };

  [[nodiscard]] Client& ClientFor(PlayerId _player);

  /// A vector walked in the order clients were first seen, for the reason `Sessions` gives: a handful of
  /// clients never justifies a hash, and a hashed container's order is what R16 forbids reaching anything.
  std::vector<Client> m_clients;
};

} // namespace Outpost
