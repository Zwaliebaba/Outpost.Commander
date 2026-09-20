#pragma once

#include "WindowsHeader.h"

#include <d3d12.h>
#include <unknwn.h>
#include <winrt/base.h>

#include "ModelDesc.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Neuron
{

class GraphicsDevice;

// A model as the geometry pass draws it (TechnicalDesign.md §6.4; ADR-011): one vertex buffer and
// one index buffer for the whole model set, a range per model, and a flat normal and a flat colour
// baked into every vertex.
//
// THE VERTEX IS SPLIT, AND THE NORMAL IS BAKED (ADR-011). §6.4 left the choice between a normal
// from the pixel shader's screen-space derivatives, as TerrainPS.hlsl takes one, and a normal baked
// per triangle by the loader. The loader bakes, because the split it costs has already been paid:
// a Frontier model is ONE COLOUR PER TRIANGLE, and a flat attribute reaches the pixel shader from
// the triangle's first vertex, so a vertex in two triangles of different colours must already be
// two vertices. Keying that same split on the normal as well as the colour costs 528 vertices
// instead of 232 over the twenty-two models in GameData\Models - 15 kB against 4 kB - and buys a
// normal that is exact, view-independent and checkable on the CPU. The derivative's sign, which
// the terrain resolves by knowing that the ground faces up, has no such answer on a model.
//
// POSITIONS BECOME WORLD UNITS HERE. A ModelDesc is in subunits, 256 to the world unit
// (NeuronCore/FixedPoint.h), because Content is where the simulation's numbers live; the camera, the
// terrain and the render view are in world units. This is the conversion, and it happens once at
// load rather than per frame.
//
// THE OUTWARD NORMAL IS cross(c - a, b - a). The placeholder set winds a front face clockwise seen
// from outside, which is what Direct3D calls front by default, and under the right-hand rule that
// winding's cross product points INTO the solid: all 264 triangles of the twenty-two models do.
// So the normal is the left-handed cross, matching the SDK's convention rather than the textbook's,
// and ModelMesh::inwardFacingTriangles counts the triangles that disagree with it so that a badly
// wound authored model is a number in the log and an assertion in a test rather than a dark face
// somebody notices in a capture.

/// One vertex of a model: a position in world units, the flat normal of its triangle, and that
/// triangle's colour. A colour with alpha zero is the team-colour slot, drawn neither lit nor
/// fogged (Lighting.h).
struct GeometryVertex
{
  float x;
  float y;
  float z;
  float nx;
  float ny;
  float nz;
  std::uint32_t color; ///< RGBA8, R in the low byte
};

/// One instance of a model: where it stands in world units, its heading as a cosine and a sine so
/// that the vertex shader turns it with two multiply-adds, and the seat's colour that the
/// team-colour slot takes.
struct GeometryInstance
{
  float x;
  float y;
  float z;
  float headingCos;
  float headingSin;
  std::uint32_t teamColor;                      ///< RGBA8, R in the low byte
  std::array<float, 3> scale{1.0f, 1.0f, 1.0f}; ///< NeuronCore/RenderView.h's InstanceScale
};

// THE INPUT LAYOUT IS WRITTEN FROM THESE OFFSETS and there is no compiler to tell it apart from a
// wrong one: a mismatched element reads whatever is at that byte and draws it, so the failure is a
// model at the wrong size or in the wrong place rather than a diagnostic. Pinned here, beside the
// struct, so that a member inserted above `scale` fails the build instead of the frame.
static_assert(offsetof(GeometryInstance, x) == 0);
static_assert(offsetof(GeometryInstance, headingCos) == 12);
static_assert(offsetof(GeometryInstance, teamColor) == 20);
static_assert(offsetof(GeometryInstance, scale) == 24);
static_assert(sizeof(GeometryInstance) == 36);

/// What BuildModelMesh makes of a ModelDesc: the split vertices, indices into them, and the two
/// numbers that say whether the model is what it claims to be.
struct ModelMesh
{
  std::vector<GeometryVertex> vertices;
  std::vector<std::uint32_t> indices;
  /// The furthest vertex from the model's origin, in world units; what an instance cull would test.
  float radiusWorldUnits;
  /// Triangles whose baked normal points at the model's centroid.
  ///
  /// A DIAGNOSTIC AND NOT A GATE, WHICH IS A CORRECTION (m1-vertical-slice/C10). ADR-011 introduced
  /// it as "a closed solid has none of these" and that is true of a CONVEX solid - the placeholder
  /// boxes and cones it was measured on. It is not true of authored geometry: a thin plate has two
  /// large faces with the centroid between them, so one of them points at the centroid however the
  /// model is wound. Measured over the twenty-two authored models it read 6,356 of 9,632 while they
  /// were inside out and 3,276 after they were corrected - it moved the right way and reached
  /// neither zero nor the total. Use signedVolumeSubunits to decide orientation; this is for a
  /// reader looking at one model.
  std::uint32_t inwardFacingTriangles;
  /// Six times the signed volume the triangles enclose, under ADR-011's outward normal.
  ///
  /// THIS IS THE ORIENTATION TEST, and it is exact for a closed mesh rather than a proxy: a model
  /// whose faces all point outward encloses a positive volume, and one wound inside out encloses
  /// the same volume negated. It caught what the centroid count could only hint at - all
  /// twenty-two authored models were inside out, every one of them decisively, where the centroid
  /// count said a mushy two thirds. A mesh with holes in it still answers, because the faces it
  /// does have dominate; what it cannot do is be fooled by a shape's concavity.
  double signedVolumeSubunits;
};

/// Splits _model's vertices by triangle colour and triangle normal and bakes both into each one.
/// Pure over the description, so the unit test needs no device.
[[nodiscard]] ModelMesh BuildModelMesh(const Outpost::ModelDesc& _model);

/// What a model occupies in the shared buffers.
struct ModelRange
{
  std::uint32_t vertexOffset;
  std::uint32_t indexOffset;
  std::uint32_t indexCount;
  float radiusWorldUnits;
};

/// No model of that id; what Find returns, and what the geometry pass skips an instance on.
inline constexpr std::uint32_t NO_MODEL = 0xFFFFFFFFu;

/// The model set on the GPU: every model built once into one vertex and one index buffer, with the
/// range of each. A render view names a model by its index here, which is what Find turns an id
/// into once, when the executable builds its table.
class ModelBuffers
{
public:
  ModelBuffers(GraphicsDevice& _device, std::span<const Outpost::ModelDesc> _models);

  [[nodiscard]] std::uint32_t Count() const noexcept
  {
    return static_cast<std::uint32_t>(m_ranges.size());
  }
  /// The index of the model with this id, or NO_MODEL.
  [[nodiscard]] std::uint32_t Find(std::string_view _id) const noexcept;
  /// _model must be below Count().
  [[nodiscard]] const ModelRange& Range(std::uint32_t _model) const noexcept
  {
    return m_ranges[_model];
  }
  [[nodiscard]] const D3D12_VERTEX_BUFFER_VIEW& VertexView() const noexcept
  {
    return m_vertexView;
  }
  [[nodiscard]] const D3D12_INDEX_BUFFER_VIEW& IndexView() const noexcept
  {
    return m_indexView;
  }
  /// Models whose faces enclose a NEGATIVE volume: wound inside out, and lit from inside.
  [[nodiscard]] std::uint32_t InsideOutModels() const noexcept
  {
    return m_insideOutModels;
  }

  [[nodiscard]] std::uint32_t InwardFacingTriangles() const noexcept
  {
    return m_inwardFacingTriangles;
  }

private:
  std::vector<std::string> m_ids;
  std::vector<ModelRange> m_ranges;
  winrt::com_ptr<ID3D12Resource> m_vertexBuffer;
  winrt::com_ptr<ID3D12Resource> m_indexBuffer;
  D3D12_VERTEX_BUFFER_VIEW m_vertexView{};
  D3D12_INDEX_BUFFER_VIEW m_indexView{};
  std::uint32_t m_inwardFacingTriangles = 0;
  std::uint32_t m_insideOutModels = 0;
};

} // namespace Neuron
