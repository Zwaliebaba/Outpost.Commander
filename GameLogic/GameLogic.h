#pragma once

// The authoritative simulation: all game state and rules. Server only (ADR-002).

#include "NeuronServer.h"
#include "GameProtocol.h"

#include <algorithm>
#include <concepts>
#include <condition_variable>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <thread>
#include <tuple>
#include <utility>

#include "Tuning.h"
#include "Map.h"
#include "PlaneVector.h"
#include "TickObserver.h"
#include "Pathfinder.h"
#include "Research.h"
#include "ShipDesign.h"
#include "Simulation.h"
#include "WorldState.h"
#include "WorldFolder.h"
#include "StressLoad.h"
#include "InProcessServer.h"
#include "MeasurementLoad.h"