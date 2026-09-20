#pragma once

// The precompiled header of GameShared: the standard library and NeuronCore, and never a platform header
// (ADR-001). Content includes no Windows, Direct3D, socket or WinRT header anywhere; the
// layering check of Build/CheckProjectFiles.py refuses one.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Assertion.h"
