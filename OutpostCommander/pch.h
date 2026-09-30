#pragma once

#include "NeuronCore.h"

#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>

// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Activation.h>

#include "NeuronClient.h"
#include "GameProtocol.h"
#include "Opponent.h"
#include "GameApp.h"
