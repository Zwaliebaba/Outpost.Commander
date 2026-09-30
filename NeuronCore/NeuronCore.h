#pragma once

// Shared by client and server, so no XAML, WinRT API or renderer headers here (ADR-002).

#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// The Windows macro family is defined here and nowhere else (AGENTS.md §4).
// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
// #define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#if !defined WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <Windows.h>
#include <hstring.h>
#include <restrictederrorinfo.h>
#include <unknwn.h>

// C++/WinRT's base, from the Windows SDK, for winrt::com_ptr and winrt::check_hresult (AGENTS.md R12, ADR-001).
// In Debug the header alone needs ole32, and check_hresult needs oleaut32 and runtimeobject, so every binary that includes
// it links all three.
#include <winrt/base.h>

#include "Debug.h"
#include "NeuronHelper.h"

#include "FileSys.h"

#pragma comment(lib, "Ws2_32.lib")

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "runtimeobject.lib")
