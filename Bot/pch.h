#pragma once

// The precompiled header of Bot, the stress harness (ADR-022). It includes the master include of the client
// library it is built from, which carries the chain below it (AGENTS.md section 2) -- and not `GameLogic`,
// which no client links (R19).

#include "GameClient.h"
