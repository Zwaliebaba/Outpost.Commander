#pragma once

#include "NeuronCore.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace Neuron
{

/// Reads a file that ships inside the package ([`ADR-021`](../Design/ADR/ADR-021-content-ships-with-the-package.md)).
///
/// **IT ASKS THE PROJECTION FOR THE PATH AND THEN READS THE FILE WITH AN ifstream, AND THE OBVIOUS
/// ALTERNATIVE DEADLOCKS.** The tidy-looking version is
/// `Package::Current().InstalledLocation().GetFileAsync(...).get()`, and M1.9 names it as the trap:
/// **the frame thread is an ASTA and blocking it on an asynchronous operation is not allowed**.
/// `ReadHostAddress` already learned this the expensive way -- the client died between opening its log
/// and writing the first line into it, leaving a zero-byte log and no window.
///
/// `InstalledLocation().Path()` is a PROPERTY rather than an async operation, so it is safe anywhere;
/// the read after it is ordinary file I/O on a path the process can see.
///
/// **THE INSTALL LOCATION IS READ-ONLY AT RUNTIME** (`manifest.json`'s target note). Nothing writes
/// beside these files; anything derived goes to `ApplicationData::Current().LocalFolder()`.
///
/// **IT IS STILL BLOCKING I/O AND IS CALLED BEFORE THE FRAME LOOP STARTS.** Four hundred kilobytes of
/// mesh at launch is not a frame's worth of work to hide; the moment something has to load DURING a
/// match, this needs a worker and a handoff rather than a bigger comment.
///
/// _relativePath is package-relative with backslashes, as the manifest states them:
/// `Assets\Meshes\Scout.cmo`.
///
/// **NEVER THROWS.** False on anything at all -- no package, no file, no permission -- and _outBytes
/// is left alone. ADR-021 names the failure this has to survive: the package not carrying the file on
/// a clean install, **which does not appear on the machine that built it**.
[[nodiscard]] bool ReadPackageFile(std::wstring_view _relativePath, std::vector<std::byte>& _outBytes) noexcept;

} // namespace Neuron
