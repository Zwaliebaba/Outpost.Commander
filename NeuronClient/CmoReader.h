#pragma once

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Neuron
{

/// A `.cmo` file, read. [`ADR-005`](../Design/ADR/ADR-005-a-mesh-is-a-cmo-file.md): a mesh is a file
/// the Visual Studio content pipeline produces, and **nothing in this tree read one until now**.
///
/// **IT IS THE ENGINE'S BECAUSE A CMO READER KNOWS NOTHING ABOUT A GAME** (R9). It returns positions,
/// normals, vertex colors and indices **in the file's own coordinates**; which axis is up and what a
/// vertex color selects are `GameClient`'s, and the conversion happens there.
///
/// **NO DirectXTK12** (R14, and ADR-005 turns on it). The format below was grounded against real
/// converter output by `Scripts/BuildMeshes.py` and this is the client's copy of that walk. The two
/// are kept honest by both asserting the same counts and extents out of `manifest.json` rather than by
/// reading each other.
///
///     UINT      mesh count
///     per mesh:
///       wstr    name
///       UINT    material count
///       per material:
///         wstr  name
///         float4 ambient, float4 diffuse, float4 specular, float specularPower, float4 emissive
///         float4x4 uv transform
///         wstr  pixel shader name
///         wstr  x 8  texture file names, all empty here and each still length-prefixed
///       BYTE    skeletal animation present
///       UINT    submesh count, then 5 x UINT each
///       UINT    index buffer count, then per buffer: UINT index count, uint16 x count
///       UINT    vertex buffer count, then per buffer: UINT vertex count, 52 bytes x count
///       UINT    skinning vertex buffer count, then per buffer: UINT vertex count, 32 bytes x count
///       float   center[3], radius, min[3], max[3]
///       when the skeletal byte is set: bones, then animation clips
///
/// **ONE FIELD IN THAT LIST DOES NOT SURVIVE MEMORY AND IS THE REASON THIS COMMENT EXISTS.** A
/// material's ambient, diffuse, specular and emissive are `float4`, not `float3`, with the specular
/// power a single float between specular and emissive. A `float3` reading walks off the end inside the
/// eight texture slots and fails several kilobytes later, somewhere unrelated.
///
/// **THE MVP USES NONE OF THE SKINNING, THE BONES OR THE CLIPS AND MUST SKIP THEM RATHER THAN REFUSE
/// THEM** (M1.9). `BuildMeshes.py` refuses a skinned file because nothing in *this content* is skinned
/// and a file that says otherwise means something upstream changed; a reader facing a file it did not
/// produce has no such standing, so it walks past them.

/// One vertex, as the MVP needs it. **Twenty-four of the file's fifty-two bytes a vertex are dead** --
/// the tangent's sixteen and the texture coordinate's eight -- and Q44 declined to repack them: at
/// 2,674 triangles the fetch is nowhere near a bottleneck and repacking costs a second format.
///
/// R8: a public aggregate.
struct CmoVertex
{
  float positionX = 0.0f;
  float positionY = 0.0f;
  float positionZ = 0.0f;

  float normalX = 0.0f;
  float normalY = 0.0f;
  float normalZ = 1.0f;

  /// The file's 32 bits, unchanged. **What the channels mean is the content's and not this reader's**
  /// -- `GameClient/HullMesh.h` is where R selects a team color and G indexes a hull tone.
  std::uint32_t color = 0;
};

/// One mesh, read whole.
///
/// R8: a public aggregate.
struct CmoMesh
{
  std::vector<CmoVertex> vertices;
  std::vector<std::uint16_t> indices;

  /// The file's own bounds, in its own coordinates. **This is what Q37's catalog row is checked
  /// against**: ADR-005 never scales a mesh at draw time, so the authored extent IS the hull's size.
  float minX = 0.0f;
  float minY = 0.0f;
  float minZ = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
  float maxZ = 0.0f;

  float centerX = 0.0f;
  float centerY = 0.0f;
  float centerZ = 0.0f;
  float radius = 0.0f;
};

/// Why a file would not read. Distinct values because the caller's response differs and because a test
/// names the case it is pinning.
enum class CmoFault : std::uint8_t
{
  None,
  /// The file ended inside a field.
  Truncated,
  /// No meshes, or more than the one this content ships. **Not a format error** -- CMO allows several
  /// -- and the reader refuses because every consumer here indexes one buffer pair.
  MeshCount,
  /// Not exactly one vertex buffer.
  VertexBufferCount,
  /// An index naming a vertex the buffer does not have. Refused before anything is drawn with it,
  /// because a truncated index does not fail, it reads the wrong vertex.
  IndexOutOfRange,
  /// The walk finished with bytes left over, which means it is out of step with the file rather than
  /// that the file is long. **This is the check that catches a format change**, and it is why the
  /// reader parses the parts it does not use instead of seeking past them.
  TrailingBytes,
  /// A count the rest of the file cannot possibly satisfy. Refused before anything is reserved for it,
  /// because the count is file-controlled and the allocation is not.
  ImpossibleCount
};

/// The largest vertex or index count this reader will reserve for before it has seen the data. The
/// handoff's largest mesh is 1,500 vertices; this is three orders of magnitude of headroom and still
/// bounds an allocation a corrupt file could otherwise ask for.
inline constexpr std::size_t CMO_MAXIMUM_ELEMENTS = 1u << 20;

/// Fifty-two bytes: position 12, normal 12, tangent 16, color 4, texture coordinate 8.
inline constexpr std::size_t CMO_VERTEX_STRIDE = 52;

/// On anything but `CmoFault::None`, _outMesh is left alone.
[[nodiscard]] CmoFault ReadCmo(std::span<const std::byte> _bytes, CmoMesh& _outMesh);

} // namespace Neuron
