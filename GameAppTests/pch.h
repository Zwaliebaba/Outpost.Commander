#pragma once

// GameAppTests drives the client's pure parts headlessly, with no window and no GPU: NeuronClient's mesh reader, and
// GameApp's model data and camera math (ADR-011, ADR-012). It can include what GameApp can, and never GameLogic
// (ADR-002).

#include "GameApp.h"

#include <CppUnitTest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
