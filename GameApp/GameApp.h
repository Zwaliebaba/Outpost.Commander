#pragma once

// The client game: presentation, selection, camera and UI state, drawn from snapshots.
// It can include GameProtocol, never GameLogic (ADR-002).

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <utility>

#include "NeuronClient.h"
#include "GameProtocol.h"

#include "ModelCatalog.h"
#include "Hardpoints.h"
#include "Camera.h"
#include "SnapshotInterpolator.h"
#include "Picking.h"
#include "PlayerControls.h"
#include "CombatEffects.h"
#include "EffectRandom.h"
#include "ParticleSystem.h"
#include "ExplosionManager.h"
#include "Starfield.h"
#include "Designer.h"
#include "Hud.h"
#include "LoadDriver.h"
#include "MatchLog.h"
#include "GameClient.h"