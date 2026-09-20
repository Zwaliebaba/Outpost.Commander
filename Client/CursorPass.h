#pragma once

#include "WindowsHeader.h"

#include <d3d12.h>
#include <unknwn.h>
#include <winrt/base.h>

#include "DescriptorHeap.h"
#include "GraphicsDevice.h"
#include "GroundRay.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Neuron
{

class GraphicsDevice;

/// A ring that lies ON the landscape and tilts with it (Design/Interface.md §4; SpeciesLook.md
/// §7.1; m1-vertical-slice/K7). It is Species's MouseHighlight disc: a world-space quad on the
/// basis the ground's normal gives, sized by the camera's distance so that it reads at the same
/// size on screen without being a billboard, and drawn twice - a blurred copy under a sharp one -
/// because that is what makes a bright ring readable over bright sand.
///
/// WHERE IT GOES IS Client/GroundRay.h's ANSWER AND NOT THIS FILE'S. Everything that can be wrong
/// about which triangle the cursor lands on, which way the ground faces there and what the sea does
/// to it is decided by arithmetic that runs on any machine and is tested there; this is the draw
/// call, which can only be looked at.
///
/// THE SAME PASS DRAWS THE FOOTPRINT GHOST (m1-vertical-slice/G1b's last line). A ghost is the same
/// thing as the ring - a quad lying on the ground, tinted - at the size of a structure's footprint
/// instead of the cursor's, so it is here rather than in a second pipeline that would differ from
/// this one only in its corners.
class CursorPass
{
public:
  /// The ring's own texture and its pre-blurred twin, decoded to RGBA8 (Tools/ImportTextures.py
  /// writes both from Species's MouseHighlight). Either may be empty, and then that half draws a
  /// single white texel rather than nothing, so that the descriptor table is always complete.
  struct Texture
  {
    std::span<const std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
  };

  CursorPass(GraphicsDevice& _device, const Texture& _sharp, const Texture& _blurred, std::uint32_t _sceneSampleCount);

  /// The ring at a hit, or nothing at a miss - "off the landscape the cursor is HIDDEN and the last
  /// position is not reused" (§4's table). _constants is the terrain pass's scene constants for
  /// this frame, which is where the view projection comes from. _tint is the meaning of §4's six
  /// cursors, as straight RGBA; _pulse scales the ring, for the placement cursor Species animates.
  void Draw(ID3D12GraphicsCommandList* _list, D3D12_GPU_VIRTUAL_ADDRESS _constants, const GroundHit& _hit, float _cameraX, float _cameraY,
            float _cameraZ, const std::array<float, 4>& _tint, float _pulse);

  /// A structure's footprint on the ground, tinted by whether it may stand there. The rectangle is
  /// in world units and axis-aligned, because a footprint is cells and cells are axis-aligned; it
  /// is lifted to _groundHeight, which the caller reads off its own landscape.
  void DrawFootprint(ID3D12GraphicsCommandList* _list, D3D12_GPU_VIRTUAL_ADDRESS _constants, float _minX, float _minZ, float _maxX,
                     float _maxZ, float _groundHeight, const std::array<float, 4>& _tint);

  /// Quads drawn on the last frame: what a capture reads to tell "hidden" from "not drawn at all".
  [[nodiscard]] std::uint32_t LastQuadsDrawn() const noexcept
  {
    return m_lastQuadsDrawn;
  }

  /// Called once a frame before the first Draw, so the counter above is this frame's.
  void Begin() noexcept
  {
    m_lastQuadsDrawn = 0;
    m_used = 0;
  }

private:
  struct Vertex
  {
    float x;
    float y;
    float z;
    float u;
    float v;
  };

  static constexpr std::uint32_t VERTICES_PER_QUAD = 6;
  /// How many quads one frame may draw: the ring and a footprint ghost, and room for a second of
  /// each. A cursor that needed more than this would be a different feature.
  static constexpr std::uint32_t MAX_QUADS = 4;
  /// The whole vertex buffer, in bytes, named ONCE. It was computed in two places - the
  /// constructor's allocation and the view the draw binds - and the second did the multiplication
  /// in 32 bits and widened the result, which is a size that is right today and silently wrong the
  /// first time a vertex grows (clang-tidy's bugprone-implicit-widening-of-multiplication-result,
  /// which is what CI caught it with). The three are here rather than in the .cpp's anonymous
  /// namespace because the size needs sizeof(Vertex) and Vertex is private.
  static constexpr std::size_t VERTEX_BUFFER_BYTES = static_cast<std::size_t>(MAX_QUADS) * VERTICES_PER_QUAD * sizeof(Vertex);

  /// Six vertices into this frame's buffer and two draws over them, the blurred pass then the
  /// sharp one. Shared by the ring and the ghost, which differ only in where the corners are.
  void Emit(ID3D12GraphicsCommandList* _list, D3D12_GPU_VIRTUAL_ADDRESS _constants, const std::array<Vertex, 6>& _corners,
            const std::array<float, 4>& _tint);
  void UploadTextures(ID3D12GraphicsCommandList* _list);

  GraphicsDevice* m_device;
  DescriptorHeap m_views;
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  /// Two pipelines and not one, because the blend mode is in the pipeline state: the dark pass is
  /// SRC_ALPHA/INV_SRC_COLOR and the bright one SRC_ALPHA/ONE (§4's table).
  winrt::com_ptr<ID3D12PipelineState> m_blurredPipeline;
  winrt::com_ptr<ID3D12PipelineState> m_sharpPipeline;
  std::array<winrt::com_ptr<ID3D12Resource>, 2> m_textures;
  std::array<winrt::com_ptr<ID3D12Resource>, 2> m_uploads;
  std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, 2> m_footprints{};
  std::array<winrt::com_ptr<ID3D12Resource>, FRAMES_IN_FLIGHT> m_vertices;
  std::array<std::uint8_t*, FRAMES_IN_FLIGHT> m_verticesMapped{};
  std::uint32_t m_used = 0; ///< Quads written into this frame's buffer
  std::uint32_t m_lastQuadsDrawn = 0;
  bool m_uploaded = false;
};

} // namespace Neuron
