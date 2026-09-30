#pragma once

// Shared by client and server, so no XAML, WinRT API or renderer headers here (ADR-002).

// The Windows macro family is defined here and nowhere else (AGENTS.md §4).
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOHELP

#include <windows.h>

// C++/WinRT's base, from the Windows SDK, for winrt::com_ptr and winrt::check_hresult (AGENTS.md R12, ADR-001).
// In Debug the header alone needs ole32, and check_hresult needs oleaut32 and runtimeobject, so every binary that includes
// it links all three.
#include <winrt/base.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "runtimeobject.lib")

#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Debug.h"
#include "NeuronHelper.h"

#include "FileSys.h"
