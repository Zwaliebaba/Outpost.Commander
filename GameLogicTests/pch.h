#pragma once

// GameLogicTests drives GameLogic headlessly, with no window and no GPU (ADR-002). A test project may include
// GameLogic because it is a test and not a client.

#include "GameLogic.h"

#include <CppUnitTest.h>
