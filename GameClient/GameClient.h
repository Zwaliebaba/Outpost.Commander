#pragma once

// GameClient -- the game code only the client runs: what a player is shown, and what they are
// allowed to ask for. It holds no simulation.
//
// The master include of this library.

#include "NeuronClient.h"

#include "GameCore.h"

// This library's own headers, so that a consumer includes this one file and gets the whole chain
// (AGENTS.md section 2). Interpolation is the arithmetic and ReplicaStore is what holds the
// snapshots it runs over; the split is what lets a suite pin the arithmetic without a clock.
#include "Camera.h"
#include "ClientFrame.h"
#include "CameraGesture.h"
#include "HullMesh.h"
#include "GroupSelection.h"
#include "HitTest.h"
#include "Interpolation.h"
#include "Selection.h"
#include "JoinState.h"
#include "OrderMarker.h"
#include "ReplicaStore.h"
#include "TapOrder.h"

namespace Outpost
{
} // namespace Outpost