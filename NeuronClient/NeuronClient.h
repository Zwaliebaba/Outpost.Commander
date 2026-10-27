#pragma once

// The client engine: what only the game client needs, such as rendering, input and audio.

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <limits>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>

// It builds on NeuronCore and knows no game concept (R9).
#include "NeuronCore.h"

#include "DirectXHelper.h"

#include <DirectXMath.h>

#include "Window.h"
#include "Bloom.h"
#include "Renderer.h"
#include "MeshData.h"
#include "TextureData.h"
#include "Mesh.h"
#include "MeshPipeline.h"
#include "GlowPipeline.h"
#include "GroundMaskPipeline.h"
#include "StarPipeline.h"
#include "GlyphAtlas.h"
#include "ScreenSegment.h"
#include "DrawList.h"
#include "TextMetrics.h"
#include "LayoutTree.h"
#include "WindowStack.h"
#include "Interaction.h"
#include "UiPipeline.h"