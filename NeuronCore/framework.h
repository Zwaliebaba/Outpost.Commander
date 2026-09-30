#pragma once

// The one header that owns the Windows macro family (AGENTS.md §4). NeuronCore sits below every other project, so every
// other project reaches <windows.h> through here. The project files define none of these and nothing else may: two
// owners of one macro is C4005, and /WX makes that fatal.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOHELP

#include <windows.h>
