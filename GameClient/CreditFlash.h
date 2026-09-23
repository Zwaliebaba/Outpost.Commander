#pragma once

#include <cstdint>

namespace Outpost
{

/// Which way the balance last moved.
enum class CreditChange : std::uint8_t
{
  None,
  Gain,
  Spend
};

/// **THE CREDITS PANEL'S CHANGE FLASH** (M2.7, `design_handoff_hud` *Motion*): a 120 x 3 rule under the
/// balance, cyan on a gain and amber on a spend, decaying exponentially -- τ 120 ms over 400 on a gain, τ 180
/// over 600 on a spend.
///
/// **IT IS THE WHOLE OF HOW INCOME IS SHOWN** (`OpenQuestions.md` Q36, ruled 2026-09-23): no rate, no window,
/// no derivation. It watches the balance the per-player block carries and says which way it moved, which is
/// the signal a player acts on -- money is arriving, money just left -- and evaluates no economy rule (R19).
///
/// **A CLOCK IN, AN ALPHA OUT, AND NOTHING ELSE** (R20): a millisecond reading is handed in, so a suite pins
/// the curve without waiting for one.
class CreditFlash
{
public:
  static constexpr std::uint64_t GAIN_DURATION_MILLISECONDS = 400;
  static constexpr std::uint64_t GAIN_TIME_CONSTANT_MILLISECONDS = 120;
  static constexpr std::uint64_t SPEND_DURATION_MILLISECONDS = 600;
  static constexpr std::uint64_t SPEND_TIME_CONSTANT_MILLISECONDS = 180;

  /// The balance as the latest update said it is. **The first reading only records**: a client that joins
  /// a match with 1,000 credits has not just gained them. An unchanged balance changes nothing, so a flash
  /// runs its course between updates that repeat the same figure.
  void Observe(std::uint32_t _credits, std::uint64_t _nowMilliseconds) noexcept;

  /// Forgets the balance, so the next reading records rather than flashes -- a rejoin into a match whose
  /// balance moved while this client was away is not a gain the player just made.
  void Reset() noexcept;

  /// Which flash is showing at _nowMilliseconds: `None` once its duration has run out.
  [[nodiscard]] CreditChange Showing(std::uint64_t _nowMilliseconds) const noexcept;

  /// Its alpha, `exp(-t / τ)` from 1 at the change to the end of its duration, then 0.
  [[nodiscard]] float Alpha(std::uint64_t _nowMilliseconds) const noexcept;

private:
  std::uint32_t m_credits = 0;
  bool m_seen = false;
  CreditChange m_change = CreditChange::None;
  std::uint64_t m_changedAtMilliseconds = 0;
};

} // namespace Outpost
