#pragma once

// NeuronCore -- the engine code that both sides run.
//
// The master include of this library, and the root of the chain: every other master include in
// the tree reaches this one, and every translation unit sees it through its project's pch.h.
//
// It is also the ONE owner of the Windows macro family (AGENTS.md section 4). Nothing else in the
// tree defines any of these and no project file passes them with /D -- two owners of one macro is
// C4005, and /WX makes that fatal.

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#ifndef NODRAWTEXT
#   define NODRAWTEXT
#endif
#ifndef NOBITMAP
#   define NOBITMAP
#endif
#ifndef NOMCX
#   define NOMCX
#endif
#ifndef NOSERVICE
#   define NOSERVICE
#endif
#ifndef NOHELP
#   define NOHELP
#endif

#include <windows.h>

#include <string_view>

namespace Neuron
{
/// The name of this library, so that a suite can prove it was compiled into the binary under test
/// and that the include path reaches this header. Delete it when the first real declaration lands.
[[nodiscard]] std::string_view CoreLibraryName() noexcept;
} // namespace Neuron