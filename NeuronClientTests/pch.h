#pragma once

// NeuronClientTests drives the client engine's CPU half on its own, with no window and no GPU (ADR-089): the mesh
// and texture readers and the glyph atlas, with NeuronCore below them and no game concept (R9).

#include "NeuronClient.h"

#include <CppUnitTest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
