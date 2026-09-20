#pragma once

#include "ReplicaObject.h"

#include "Deposit.h"
#include "Landscape.h"
#include "Placement.h"

#include "ContentTree.h"

#include "FogGrid.h"

#include <cstdint>
#include <map>
#include <span>

// Whether a structure may stand where the commander is pointing, answered from the REPLICA
// (Design/Interface.md §7.1; m1-vertical-slice/K4 and G1b's footprint ghost).
//
// WHY THE CLIENT CANNOT SIMPLY CALL CheckPlacement. That function takes a World and a Seat - the
// simulation's own containers - and a client holds neither. It holds a map of the structures its
// commander can see, a fog grid of what he has explored, and a Landscape it generated for itself
// from the definition the join carried (TechnicalDesign.md §5.2: the terrain under an unscouted
// base is not public, so the client's landscape is its own and never the host's).
//
// AND WHY IT MUST NOT WRITE THE RULE AGAIN. The ground's three rules are shared literally:
// Sim/Placement.h's CheckFootprintGround is called from here and from CheckPlacement, so the
// steepest-cell reading of the slope - which that header spends thirty lines justifying, and which
// differs from the average-gradient reading exactly along cliff edges - cannot drift between the
// ghost and the order. What is written here is the two questions whose ANSWERS live in different
// containers on the two sides: what stands on the ground, and what this commander has explored.
//
// IT IS AN ANSWER ABOUT WHAT HE CAN SEE, AND THE HOST STILL DECIDES. A ghost drawn green is the
// client saying "nothing I know of refuses this"; the order is judged again by the simulation,
// which knows about the building this commander has never scouted. That is not a fault to be fixed
// by sending him more - it is §5.2 - and the refusal that comes back is what Design/Interface.md
// §6's warning line is for.

namespace Outpost
{

/// What the rule is judged against. It names the two things it reads from the replica rather than
/// taking the replica, so that what this function can see is written down and a test can hand it
/// exactly that. The caller passes `&replica.Structures(), replica.Fog(), replica.FogCellsPerSide()`
/// and nothing is copied.
///
/// An empty fog is a commander who has been sent none yet - before the first frame - and is read
/// as no answer rather than as nothing explored, which would grey out the whole landscape for the
/// one frame between the join and the fog arriving. The deposits are the field the client builds
/// from the definition it joined with, or null when it has not: an extractor is then refused
/// nowhere rather than everywhere, which is the same absence CheckPlacement treats a null
/// DepositField as.
struct PreviewQuery
{
  const std::map<std::uint32_t, ReplicaStructure>* structures;
  std::span<const FogState> fog;
  std::uint32_t fogCellsPerSide;
  const Landscape* landscape;
  const ContentTree* content;
  const DepositField* deposits = nullptr;
};

/// Where a structure of _structureRow would stand if it were placed with its lowest cell here.
[[nodiscard]] Footprint PreviewFootprint(const ContentTree& _content, std::uint32_t _structureRow, std::uint32_t _cellX,
                                         std::uint32_t _cellY) noexcept;

/// The placement rule as this commander's own information answers it, in the same fault order
/// CheckPlacement reports: off the landscape, occupied, water, too steep, unexplored, not on a
/// deposit. Accepted is the only value the ghost draws as legal.
///
/// IT TAKES THE ROW AND NOT JUST THE FOOTPRINT, because the last rule is about WHAT is being
/// built: an extractor stands on a deposit and nothing else does, and a one-cell footprint is not
/// enough to tell an extractor from a tower. A row this build does not have is OffLandscape rather
/// than a crash - a panel offering a button for a structure the tables do not carry has a worse
/// fault than this answer.
[[nodiscard]] PlacementFault PreviewPlacement(std::uint32_t _structureRow, std::uint32_t _cellX, std::uint32_t _cellY,
                                              const PreviewQuery& _query);

} // namespace Outpost
