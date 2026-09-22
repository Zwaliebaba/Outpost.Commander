#pragma once

#include "NeuronCore.h"

#include <string>
#include <string_view>

namespace Neuron
{

/// ADR-008: the host address is configuration, and configuration is a one-line file.
///
/// **THE SPLIT IN THIS FILE IS THE SAME ONE R21 FORCED ON THE GESTURE SEAM.** Reading
/// `ApplicationData::Current().LocalFolder()` needs a packaged application, and nothing in this tree
/// can construct one -- the two client suites are DESKTOP test DLLs. So the half that PARSES is a
/// pure function over a string, with a suite over it, and the half that READS is three lines in the
/// .cpp that a test never reaches. The fallback is the part that has to be right, and the fallback
/// is the part that is tested.

/// ADR-008's compiled-in default. Loopback, because the development machine is the host until the
/// day a Surface Pro is pointed at a LAN address -- which is a file being dropped rather than a
/// rebuild, and that is the whole point of the decision.
inline constexpr std::string_view DEFAULT_HOST_ADDRESS = "127.0.0.1";

/// ADR-008's file name in `LocalState`.
inline constexpr std::wstring_view HOST_ADDRESS_FILE_NAME = L"host.txt";

/// The address a file's contents name, or the default when they name none.
///
/// WHAT COUNTS AS "NAMING NONE" IS DELIBERATELY WIDE: absent, empty, whitespace, or a first line
/// that is blank. A file that exists but says nothing usable is the same situation as no file, and
/// the alternative -- refusing to start, or trying to connect to an empty string -- turns a typo
/// into a client that cannot be run at all on a machine that may not have a keyboard attached.
///
/// **IT TAKES THE FIRST LINE AND TRIMS IT.** A text file dropped by a person has a trailing newline
/// and often a trailing space, and neither is part of an address. Nothing beyond the first line is
/// read, so a note under the address is free.
///
/// **IT DOES NOT VALIDATE THE ADDRESS.** Whether the text names a reachable host is the transport's
/// answer and it arrives as a connection failure with a message; a syntax check here would be a
/// second, worse parser sitting in front of the real one, and it would have to know about IPv6 and
/// host names to avoid rejecting things that work.
[[nodiscard]] std::string HostAddressFromFileContents(std::string_view _contents);

/// The address this run should use: `LocalState\host.txt` when it is there and readable, the
/// default otherwise.
///
/// **NEVER THROWS AND NEVER FAILS.** Every way this can go wrong -- no file, no permission, a
/// folder that is not there yet on a first run -- ends at the default, because a client that will
/// not start is a worse outcome than a client pointed at loopback.
[[nodiscard]] std::string ReadHostAddress() noexcept;

} // namespace Neuron
