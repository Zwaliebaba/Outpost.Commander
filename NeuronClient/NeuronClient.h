#pragma once

// NeuronClient -- the engine code only the client runs: presentation, input and the frame loop.
//
// The master include of this library. It includes the master include of what this library is
// built on, so that a consumer includes this one file and gets the whole chain.

#include "NeuronCore.h"

// This library's own headers (AGENTS.md section 2).
#include "FitTransform.h"
#include "FrameStatistics.h"
#include "GestureArithmetic.h"
#include "InputEvent.h"
#include "WindowMetrics.h"

// Every class here declares its Direct3D state and DEFINES it in its .cpp, so that <d3d12.h> and
// <dxgi1_6.h> reach neither this chain nor the desktop suites below it.
#include "GraphicsDevice.h"
#include "InterfacePass.h"
#include "PresentStep.h"
#include "PointSprites.h"
#include "Blackbody.h"
#include "CmoReader.h"
#include "StarField.h"
#include "MeshBuffer.h"
#include "MeshPass.h"
#include "PackageFile.h"
#include "SceneTarget.h"
#include "SessionToken.h"
#include "SwapChain.h"
#include "WorldPass.h"

// This library's own headers, so that a consumer includes this one file and gets the whole chain
// (AGENTS.md section 2). DatagramTransport.h and GestureSeam.h deliberately pull in no C++/WinRT
// projection header: the two client suites are DESKTOP test DLLs and have no business compiling
// one. GestureSeam is why GestureArithmetic is a separate header above it -- everything a suite
// has to reach lives there, and this one holds the half that cannot be tested at all.
#include "DatagramTransport.h"
#include "GestureSeam.h"
#include "HostAddress.h"
#include "PacketQueue.h"

namespace Neuron
{
} // namespace Neuron