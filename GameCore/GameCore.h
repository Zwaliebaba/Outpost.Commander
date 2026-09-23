#pragma once

// GameCore -- the game rules that both sides need: what the host validates with and what the
// client is allowed to predict against.
//
// The master include of this library.

#include "NeuronCore.h"

// This library's own headers (AGENTS.md section 2): a consumer includes this one file and gets
// the whole chain.
#include "Catalog.h"
#include "DerivedStats.h"
#include "Design.h"
#include "Entity.h"
#include "EntityRecord.h"

#include "Command.h"
#include "Join.h"
#include "Layout.h"
#include "Generator.h"

#include "SizeClass.h"
#include "Snapshot.h"

namespace Outpost
{
} // namespace Outpost