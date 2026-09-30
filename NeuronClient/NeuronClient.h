#pragma once

// The client engine: what only the game client needs, such as rendering, input and audio.

#include <array>
#include <bitset>
#include <span>

// It builds on NeuronCore and knows no game concept (R9).
#include "NeuronCore.h"

#include "DirectXHelper.h"

#include <DirectXMath.h>

#include "Window.h"
#include "Renderer.h"
#include "MeshData.h"
#include "Mesh.h"
#include "MeshPipeline.h"
