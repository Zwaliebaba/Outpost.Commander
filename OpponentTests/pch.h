#pragma once

// OpponentTests drives the AI's library on its own (ADR-075): its settings and what the --ai-matches switch reads.
// The AI playing the real server is GameLogicTests' (ADR-020).

#include "Opponent.h"

#include <CppUnitTest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
