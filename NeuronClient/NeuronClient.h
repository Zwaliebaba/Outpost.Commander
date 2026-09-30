#pragma once

// The client engine: what only the game client needs, such as rendering, input and audio.
// It builds on NeuronCore and knows no game concept (R9).

#include "NeuronCore.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <array>

#include "Window.h"
#include "Renderer.h"
