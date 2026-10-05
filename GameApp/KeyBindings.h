#pragma once

namespace Outpost
{
// The keys the game reads that its interface names, in one place, so that what a button's cap says cannot disagree with what
// the input code does (task 15.2). Each is a Windows virtual-key code; a letter's is its capital.

// The keys that open and close the designer, the production window and the research window (Phase 1 design §12), clear of
// the orders' A and S, the camera's Q and E and arrows, and the control groups' digits.
inline constexpr std::uint8_t KEY_DESIGNER = 'D';
inline constexpr std::uint8_t KEY_PRODUCTION = 'P';
inline constexpr std::uint8_t KEY_RESEARCH = 'R';

// A letter key as a button's cap shows it: the letter.
[[nodiscard]] inline std::string KeyCap(std::uint8_t _key)
{
  return std::string(1, static_cast<char>(_key));
}
} // namespace Outpost
