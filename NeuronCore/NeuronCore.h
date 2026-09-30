#pragma once

// Shared by client and server, so no WinRT, XAML or renderer headers here (ADR-002).

// The Windows macro family is defined here and nowhere else (AGENTS.md §4).
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOHELP

#include <windows.h>

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

using namespace Neuron;
