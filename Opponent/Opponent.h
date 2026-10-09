#pragma once

// The AI player, and a seat's deputy. Each is a client: it reads its player's snapshot and sends
// commands, and it can include only GameProtocol, never GameLogic (ADR-002).

#include "GameProtocol.h"

#include <array>
// AiMatchesOptions.h names the AI-against-AI matches' settings and log by path (ADR-063).
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <tuple>
#include <utility>

#include "AiSettings.h"
#include "AiPlayer.h"
#include "AiEmpire.h"
#include "Deputy.h"
#include "MatchupPlayer.h"
#include "AiMatchesOptions.h"
