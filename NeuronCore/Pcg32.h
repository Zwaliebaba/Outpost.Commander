#pragma once

#include <cstdint>

namespace Neuron
{

/// The tree's one source of randomness, and ADR-002 means "one" literally: seeded from the match,
/// written out here, and never joined by a second generator somewhere convenient.
///
/// PCG32 -- `pcg_setseq_64_xsh_rr_32`. A 64-bit LCG whose output is permuted down to 32 bits, so
/// the state is twice the output width and the low-bit weakness every bare LCG has does not reach
/// a caller. It is a dozen lines, it is specified precisely enough to reimplement, and
/// Pcg32Tests pins its first thousand outputs against a table -- which is what makes "pinned"
/// mean something rather than being a claim about a library nobody read.
///
/// R4: `Pcg32`, not `PCG32`. An acronym capitalizes as a word.
///
/// NOT `std::mt19937` OR ANY OTHER STANDARD ENGINE, and the reason is narrower than it looks: the
/// engines ARE specified bit-for-bit and would have been fine. The **distributions** are not --
/// `std::uniform_int_distribution` may produce different values on two implementations from the
/// same engine state -- and an engine whose companion distribution cannot be used is an engine
/// half of whose interface is a trap. NextBelow below is the replacement, and it is pinned.
///
/// TWO CONSUMERS OF ONE STREAM IS THE FAILURE THIS CLASS IS SHAPED AROUND. R23 has both sides
/// running the world generator, and if the generator draws from the engine the simulation is also
/// drawing from, the two advance one stream and fall out of step at the first generated thing.
/// The `_stream` constructor argument is the answer: two Pcg32 objects with the same seed and
/// different streams are independent sequences from one match seed. **Which stream number belongs
/// to which consumer is not decided here** -- that derivation is the generator's to write down
/// when it arrives (M2), and this class exists to make writing it down possible.
class Pcg32
{
public:
  /// _seed is the match seed. _stream selects one of 2^63 independent sequences from it; two
  /// generators differing only in _stream never produce the same output sequence.
  explicit Pcg32(std::uint64_t _seed, std::uint64_t _stream = 0) noexcept;

  /// The next 32 bits. Every other draw in this class is built on exactly this one.
  [[nodiscard]] std::uint32_t Next() noexcept;

  /// Uniform over [0, _bound), WITHOUT MODULO BIAS. A bare `Next() % _bound` is biased whenever
  /// _bound does not divide 2^32 -- the low remainder values come up once more often than the
  /// high ones, by a margin that is invisible in a handful of draws and structural over a match's
  /// worth of them. This rejects the unrepresentable prefix of the range first, which costs an
  /// occasional extra draw and is exact.
  ///
  /// The rejection loop is deterministic: it consumes a fixed number of draws for a given stream
  /// position, so it reproduces from the seed like everything else here (R16).
  ///
  /// _bound of 0 returns 0. There is no uniform value over an empty range, and the alternative is
  /// a division by zero inside the simulation.
  [[nodiscard]] std::uint32_t NextBelow(std::uint32_t _bound) noexcept;

  /// Uniform over [_lowest, _highest], INCLUSIVE at both ends, which is what a caller placing a
  /// thing on a grid actually wants. _lowest must not exceed _highest.
  [[nodiscard]] std::int32_t NextInRange(std::int32_t _lowest, std::int32_t _highest) noexcept;

private:
  /// Knuth's, and the one PCG32 is specified with. Changing it makes this a different generator
  /// and every pinned output in the suite wrong, which is the point of pinning them.
  static constexpr std::uint64_t MULTIPLIER = 6364136223846793005ull;

  std::uint64_t m_state = 0;

  /// Always odd -- that is what makes the LCG full-period -- which is why the constructor shifts
  /// the stream up and sets the low bit rather than storing it as given.
  std::uint64_t m_increment = 1;
};

} // namespace Neuron
