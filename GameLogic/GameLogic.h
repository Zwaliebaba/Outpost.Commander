#pragma once

// The authoritative simulation: all game state and rules. Server only (ADR-002).

#include "NeuronServer.h"
#include "GameProtocol.h"

#include <algorithm>
#include <optional>
#include <span>

#include "Tuning.h"
#include "Map.h"
#include "PlaneVector.h"
#include "Pathfinder.h"
#include "ShipDesign.h"
#include "Simulation.h"
#include "StressLoad.h"
#include "InProcessServer.h"
#include "MeasurementLoad.h"