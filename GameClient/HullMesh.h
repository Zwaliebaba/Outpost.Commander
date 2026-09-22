#pragma once

#include "MeshCatalog.g.h"

#include "GameCore.h"
#include "NeuronClient.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// **THE ONLY PART OF THE MESH PATH THAT KNOWS WHAT A `Scout` IS** (R9). `NeuronClient/CmoReader.h`
/// reads a file and knows nothing about a game; `MeshCatalog.g.h` is generated from the manifest and
/// knows nothing about a hull. This is the map between them, and it is written by hand on purpose --
/// a generator that knew a `Scout` was a hull would be the manifest deciding the catalog.

/// A vertex as the world pass wants it: **in the camera's coordinates, not the file's.**
///
/// R8: a public aggregate.
struct HullVertex
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  float normalX = 0.0f;
  float normalY = 0.0f;
  float normalZ = 1.0f;

  /// **THE TEAM SELECTOR, 0 TO 1.** The file's red channel: 0 takes the hull palette, 255 takes the
  /// owning player's colour. Values between are reserved and unused in the MVP.
  float teamBlend = 0.0f;

  /// **THE HULL TONE, 0 TO 1.** The file's green channel, which quantizes to DEEP at 0, BASE at 128
  /// and EDGE at 255 -- so the shader interpolates a three-stop ramp rather than storing a colour.
  float hullTone = 0.0f;
};

/// One loaded hull.
///
/// R8: a public aggregate.
struct HullMesh
{
  std::vector<HullVertex> vertices;
  std::vector<std::uint16_t> indices;

  /// In world units, along the longest axis, from the file -- which is what Q37's catalog row states
  /// and what `Scripts/CheckMeshes.py` compares the two of.
  float longestUnits = 0.0f;
};

/// **THE HANDOFF AUTHORS Y UP AND THIS CAMERA IS Z UP**, and `GameClient/Camera.cpp` said this
/// conversion "belongs where the mesh is loaded rather than smuggled into the transform". This is
/// where.
///
/// The authored frame is left-handed with **+Y up and +Z forward**; the world is right-handed with
/// **+Z up** and the plane at Z = 0, and heading zero looks along +X (`GameClient/Camera.h`). So the
/// nose goes to +X, the top goes to +Z, and the remaining axis takes the sign that makes the whole
/// thing a reflection:
///
///     world x =  authored z      the nose
///     world y = -authored x      **negated, and this is the handedness**
///     world z =  authored y      the top
///
/// **A REFLECTION REVERSES TRIANGLE WINDING**, so `LoadHullMesh` reverses each triangle's indices as
/// it copies them. The alternative -- flipping the rasterizer's cull mode -- would make every other
/// thing drawn in the world pass wrong instead, which is a worse trade for the same arithmetic.
[[nodiscard]] HullVertex ToWorldVertex(const Neuron::CmoVertex& _vertex) noexcept;

/// Which mesh a hull draws. **Empty for a hull nothing authored**, which is the `Cruiser`: it is cut
/// from the MVP (`GameDesign.md` section 6), so no mesh exists and M4 authors one to the catalog's
/// stated size rather than the other way round.
[[nodiscard]] std::string_view MeshNameForHull(HullId _hull) noexcept;

/// And for a design, through its hull -- which is the call the renderer actually makes, because a
/// snapshot carries a design identity and not a hull (ADR-003).
[[nodiscard]] std::string_view MeshNameForDesign(DesignId _design) noexcept;

/// **THE THREE THAT SHIP AT M1.9**: `Scout`, `Frigate` and the station. The other ten delivered
/// meshes are M2's modules and asteroids, and they are in the catalog already -- this is the set the
/// client uploads today, so that a buffer is not allocated for geometry nothing draws.
[[nodiscard]] std::span<const std::string_view> MeshesShippedAtM1() noexcept;

/// Turns a read file into what the world pass uploads. Returns false on an empty mesh or one whose
/// index count is not a multiple of three -- a triangle list that is not whole is a file this build
/// cannot draw, and drawing two thirds of a triangle is worse than drawing nothing.
[[nodiscard]] bool LoadHullMesh(const Neuron::CmoMesh& _read, float _longestUnits, HullMesh& _outMesh);

/// The palette entry a tone index selects, interpolated. **Three stops, not three colours**: the G
/// channel is quantized to 0, 128 and 255 by the content, and a shader that interpolated between them
/// would draw the same thing -- so this is the CPU-side statement of what the shader does, and the
/// suite pins the two ends and the middle against `manifest.json`'s hex.
void HullToneColor(float _tone, float& _outRed, float& _outGreen, float& _outBlue) noexcept;

/// The colour a player's ships take. **Player one is `OWN`**, which is the cyan the handoff's plates
/// were composed against; a player past the four the design has takes the hull palette instead of
/// reading off the end.
void TeamColor(PlayerId _player, float& _outRed, float& _outGreen, float& _outBlue) noexcept;

/// What the pixel stage is told about the look, from the generated catalog. **The palette and the
/// light rig are the content's, not this file's** -- both come out of `manifest.json` through
/// `MeshCatalog.g.h`, so a hull tone that moved in the handoff moves here without anybody editing
/// code.
[[nodiscard]] Neuron::MeshPass::Look ShipLook() noexcept;

/// One entity's per-instance data.
///
/// **THE POSITION AND HEADING ARE THE INTERPOLATED ONES AND ARE THE CALLER'S** -- M0.19 decides
/// where a replica is drawn and this only converts. The heading's cosine and sine are computed here
/// rather than in the shader, because a `sincos` per vertex to turn one entity is work the instanced
/// arrangement exists to avoid.
[[nodiscard]] Neuron::MeshInstance InstanceFor(float _worldX, float _worldY, Neuron::Angle _heading, PlayerId _owner) noexcept;

} // namespace Outpost
