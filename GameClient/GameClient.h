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
#include "AsteroidMesh.h"
#include "GroupSelection.h"
#include "HitTest.h"
#include "HudLayout.h"
#include "PanelHitTest.h"
#include "Panels.h"
#include "Interpolation.h"
#include "Selection.h"
#include "SkyLook.h"
#include "JoinState.h"
#include "FieldView.h"
#include "OrderMarker.h"
#include "ReplicaStore.h"
#include "TapOrder.h"

// ADR-022's stress harness: the decisions the `Bot` executable must not hold (R20).
#include "BotPolicy.h"
#include "ChurnSchedule.h"
#include "FloodSchedule.h"
#include "StressReport.h"

namespace Outpost
{
} // namespace Outpost