#pragma once

// The AI player. It is a client: it reads its player's snapshot and sends commands, and it
// can include only GameProtocol, never GameLogic (ADR-002).

#include "GameProtocol.h"

#include <array>
#include <limits>
#include <map>
#include <optional>
#include <tuple>
#include <utility>

#include "AiSettings.h"
#include "AiPlayer.h"
