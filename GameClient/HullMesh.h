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
/// **A REFLECTION REVERSES TRIANGLE WINDING, AND THE VIEW REFLECTS IT BACK.** `ViewProjection` maps
/// this right-handed world into Direct3D's left-handed view space, which is a second reflection, so
/// the two cancel and `LoadHullMesh` copies the indices as authored. Reversing them here draws every
/// hull inside out under back-face culling.
[[nodiscard]] HullVertex ToWorldVertex(const Neuron::CmoVertex& _vertex) noexcept;

/// **THE SAME MAPPING, FOR A BARE DIRECTION.** The light rig in `manifest.json` is stated in the
/// authored frame like everything else the handoff delivers, and **a light that is not converted
/// with the geometry points somewhere else entirely** -- the key at `[-0.42, 0.8, 0.38]` reads as
/// high and front-left in an authored Y-up frame and as very nearly HORIZONTAL in this Z-up one, so
/// the top face of a station is left to ambient and the whole hull goes shapeless.
///
/// That is the defect this function exists to make impossible, and it is why the conversion is one
/// expression both callers share rather than two that agree today.
void ToWorldDirection(float _authoredX, float _authoredY, float _authoredZ, float& _outX, float& _outY, float& _outZ) noexcept;

/// Which mesh a hull draws. **Empty for a hull nothing authored**, which is the `Cruiser`: it is cut
/// from the MVP (`GameDesign.md` section 6), so no mesh exists and M4 authors one to the catalog's
/// stated size rather than the other way round.
[[nodiscard]] std::string_view MeshNameForHull(HullId _hull) noexcept;

/// And for a design -- which is the call the renderer actually makes, because a snapshot carries a design
/// identity and not a hull (ADR-003). **Through its hull, except a module**, which is drawn by its level
/// (M2.10b).
[[nodiscard]] std::string_view MeshNameForDesign(DesignId _design) noexcept;

/// How many meshes a design can draw with: M1.9's three hulls, M2.10b's four module levels and M4.4's Cruiser.
inline constexpr std::size_t SHIPPED_MESH_COUNT = 8;

/// **EVERY MESH A DESIGN DRAWS WITH**, which is the set the client uploads and instances: `Scout`, `Frigate`,
/// the station and one mesh per module level. The bare `ModuleFrame` and the asteroids are not in it.
[[nodiscard]] std::span<const std::string_view> ShippedMeshes() noexcept;

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
