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
#include "Matchup.h"
#include "PlaneVector.h"
#include "TickObserver.h"
#include "Pathfinder.h"
#include "Research.h"
#include "ShipDesign.h"
#include "Simulation.h"
#include "AwayReports.h"
#include "UniverseState.h"
#include "UniverseFolder.h"
#include "UniverseSettings.h"
#include "SeatController.h"
#include "UniverseLog.h"
#include "StressLoad.h"
#include "InProcessServer.h"
#include "UniverseRun.h"
#include "MeasurementLoad.h"