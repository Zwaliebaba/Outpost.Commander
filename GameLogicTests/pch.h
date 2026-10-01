#pragma once

// GameLogicTests drives GameLogic headlessly, with no window and no GPU (ADR-002). A test project may include
// GameLogic because it is a test and not a client, and Opponent so that the AI plays the real server (ADR-020).

#include "GameLogic.h"
#include "Opponent.h"

#include <CppUnitTest.h>

#include <filesystem>
#include <fstream>
#include <sstream>