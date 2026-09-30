#pragma once

// The client game: presentation, selection, camera and UI state, drawn from snapshots.
// It can include GameProtocol, never GameLogic (ADR-002).

#include <map>
#include <optional>
#include <utility>

#include "NeuronClient.h"
#include "GameProtocol.h"

#include "ModelCatalog.h"
#include "Camera.h"
#include "GameClient.h"
