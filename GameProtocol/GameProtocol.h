#pragma once

// What crosses the client/server boundary: commands, snapshots, entity IDs and the factory for
// the in-process server. Plain data, with no pointers into server state (ADR-002), and the pure
// design math both sides derive a design's stats with (ADR-017).

#include "NeuronCore.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <compare>
#include <numbers>
#include <variant>

#include "Id.h"
#include "PlanePosition.h"
#include "StructureKind.h"
#include "Command.h"
#include "Snapshot.h"
#include "DesignStats.h"
#include "Transport.h"
#include "Server.h"
