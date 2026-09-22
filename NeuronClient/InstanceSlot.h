#pragma once

#include "NeuronCore.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Neuron
{

/// Which of the clients running side by side on this machine this one is, so each keeps its own files.
///
/// **TWO INSTANCES OF ONE PACKAGE SHARE ONE `LocalState`**, and that is what makes a second instance
/// (`Design/Plan/M1-the-fleet.md` M1.15) worse than useless without this: both would read the same
/// session token, the second would present the first's, and the host would recognize it as the first
/// player returning at a new endpoint (ADR-013) -- one seat, two clients, and the first one silently
/// deaf. Each instance claims a slot and names its files by it.
///
/// **THE SPLIT IS `SessionToken.h`'s.** The name a slot gives a file is pure and has a suite; claiming
/// the slot needs a packaged application and is a handful of lines no test reaches.

/// As many slots as a match has players (`GameDesign.md` section 2). A client past the last one shares
/// the last one's files, which is five clients on one machine and not a case anybody runs.
inline constexpr std::uint32_t INSTANCE_SLOT_COUNT = 4;

/// The file this slot uses in place of _fileName. **Slot zero is the name unchanged**, so a single
/// client -- the ordinary case -- keeps every file where it always was; slot one of `session.txt` is
/// `session-1.txt`. The suffix goes before the last extension, or at the end when there is none.
[[nodiscard]] std::wstring InstanceFileName(std::wstring_view _fileName, std::uint32_t _slot);

/// The lowest slot no other running instance holds, claimed for the life of this process. **Never
/// fails**: when the platform will not register a key, the answer is slot zero, which is exactly what
/// a single-instance client always was.
[[nodiscard]] std::uint32_t ClaimInstanceSlot() noexcept;

} // namespace Neuron
