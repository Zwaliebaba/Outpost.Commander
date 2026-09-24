#pragma once

#include "GameCore.h"

#include "NeuronClient.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// **HOW LONG A WRECK STAYS: TEN SECONDS** (M3.4). Long enough to see where a fight went, and short enough
/// that a field of them never hides what is still alive. It darkens the whole time and is gone at the end.
/// **Not tuned.**
inline constexpr std::uint64_t WRECK_LIFETIME_MILLISECONDS = 10000;

/// How long the burst of light at a death lasts, from the moment the removal arrives.
inline constexpr std::uint64_t BLAST_MILLISECONDS = 500;

/// The most wrecks held. Past it the oldest goes first, so a big fight costs old wrecks rather than new ones.
inline constexpr std::size_t MAX_WRECKS = 128;

/// One thing that died, as it was last drawn. R8: a public aggregate.
struct Wreck
{
  /// The record the removal took out of the store: where it was, which way it faced, what and whose it was.
  EntityRecord last;

  /// When its removal arrived.
  std::uint64_t diedMilliseconds = 0;
};

/// **THE WRECKS** (M3.4). **Presentation and not simulation**: spawned from a removal and never from the store
/// forgetting, decaying on the client's own clock, and the host never knows one exists.
class WreckSet
{
public:
  WreckSet();

  /// A wreck for each record a removal took out, arrived at _nowMilliseconds.
  void Spawn(std::span<const EntityRecord> _removed, std::uint64_t _nowMilliseconds);

  /// Drops every wreck whose lifetime has run out by _nowMilliseconds.
  void Expire(std::uint64_t _nowMilliseconds) noexcept;

  void Clear() noexcept;

  [[nodiscard]] std::span<const Wreck> Wrecks() const noexcept
  {
    return m_wrecks;
  }

private:
  std::vector<Wreck> m_wrecks;
};

/// **HOW A WRECK IS DRAWN**: the hull it was, dark and tumbling slowly about its center, darkening to nothing
/// over its lifetime. The owner's color goes gray, since a wreck belongs to nobody.
[[nodiscard]] Neuron::MeshInstance WreckInstance(const Wreck& _wreck, std::uint64_t _nowMilliseconds) noexcept;

/// **THE BURST AT A DEATH**, appended to _ioBeams: short rays out of the wreck, hot white to orange, growing to
/// the hull's size and fading over `BLAST_MILLISECONDS`. Nothing for a wreck older than that.
void AppendBlasts(const WreckSet& _wrecks, std::uint64_t _nowMilliseconds, float _unitsPerPixel,
                  std::vector<Neuron::BeamInstance>& _ioBeams);

} // namespace Outpost
