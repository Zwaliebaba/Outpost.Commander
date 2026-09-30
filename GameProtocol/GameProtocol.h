#pragma once

// What crosses the client/server boundary: commands, snapshots, entity IDs and the factory for
// the in-process server. Plain data only, with no pointers into server state (ADR-002).

#include "NeuronCore.h"

#include <compare>
#include <variant>

#include "Id.h"
#include "PlanePosition.h"
#include "StructureKind.h"
#include "Command.h"
#include "Snapshot.h"
#include "Transport.h"
#include "Server.h"
