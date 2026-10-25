#pragma once

// NeuronCoreTests drives the engine's shared library on its own (ADR-089): it can include NeuronCore and nothing
// above it, so no test of the engine leans on a game concept (R9).

#include "NeuronCore.h"

#include <CppUnitTest.h>

#include <filesystem>
#include <fstream>
