#pragma once

#include "NeuronCore.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Neuron
{

/// ADR-013's session token, kept across a relaunch so a returning client gets its slot back.
///
/// **THE SPLIT IS `HostAddress.h`'s, AND FOR THE SAME REASON.** Reaching `LocalState` needs a
/// packaged application and neither client suite is one -- both are desktop test DLLs. So the half
/// that PARSES and the half that FORMATS are pure functions with a suite over them, and the half
/// that touches the file system is a handful of lines in the .cpp that no test reaches.
///
/// **IT IS ONE FILE MORE IN A FOLDER THAT ALREADY HAS ONE.** ADR-008 put `host.txt` there and this
/// sits beside it; the difference is that this one is WRITTEN by the client rather than dropped by
/// a person, which is why it is hexadecimal rather than something anybody has to read.

/// The file name in `LocalState`.
inline constexpr std::wstring_view SESSION_TOKEN_FILE_NAME = L"session.txt";

/// What a client that has never joined sends (`GameCore/Join.h` spells the same zero; this one is
/// the engine's side of the seam and does not include the game).
inline constexpr std::uint64_t NO_TOKEN = 0;

/// The token a file's contents name, or zero when they name none.
///
/// **SIXTEEN HEXADECIMAL DIGITS, AND ANYTHING ELSE IS ZERO.** Absent, empty, blank, too long, too
/// short or containing a character that is not a digit -- every one of them means "I have no
/// token", which is a state the protocol already has to handle on a first run. There is no failure
/// mode here that is worth more than rejoining as a new client.
///
/// It is deliberately NOT decimal: a 64-bit token in decimal is twenty digits that all look alike,
/// and a fixed sixteen makes a truncated file detectable by length alone.
[[nodiscard]] std::uint64_t SessionTokenFromFileContents(std::string_view _contents);

/// The inverse, and the exact shape the reader expects: sixteen lower-case hexadecimal digits with
/// no prefix and no newline.
[[nodiscard]] std::string SessionTokenToFileContents(std::uint64_t _token);

/// The token this run should present: `LocalState\session.txt` when it is there and readable, zero
/// otherwise. **_instanceSlot names the file** (`InstanceSlot.h`), because two clients on one machine
/// that shared a token would share a seat; slot zero is `session.txt` itself.
///
/// **NEVER THROWS AND NEVER FAILS**, for the reason `ReadHostAddress` does not: a client that will
/// not start is worse than a client that joins as somebody new.
[[nodiscard]] std::uint64_t ReadSessionToken(std::uint32_t _instanceSlot) noexcept;

/// Keeps a token for next time. False when it could not be written, which the caller may ignore --
/// **the cost of a failed write is a client that takes a second slot after a relaunch**, and
/// ADR-013 names that hazard rather than pretending the write cannot fail.
///
/// Writing `NO_TOKEN` removes the file, so "I was refused" and "I have never joined" are one state
/// rather than two.
bool WriteSessionToken(std::uint64_t _token, std::uint32_t _instanceSlot) noexcept;

} // namespace Neuron
