#pragma once

// GameAppTests drives GameApp's pure parts headlessly, with no window and no GPU: its model data, camera math and
// interface (ADR-011, ADR-012). NeuronClient's own are NeuronClientTests' (ADR-075). It can include what GameApp can, and
// never GameLogic (ADR-002).

#include "GameApp.h"

#include <CppUnitTest.h>

#include <filesystem>
#include <fstream>
#include <iterator>