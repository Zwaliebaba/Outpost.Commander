#pragma once

// The precompiled header of OutpostCommander, the packaged client.
//
// The game libraries first, so that the Windows macro family is set by its one owner before
// <windows.h> is reached, and the C++/WinRT projection after it.

#include "GameClient.h"

#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Core.h>
