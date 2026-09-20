#pragma once

#include "WindowsHeader.h"

#include <d3d12.h>
#include <unknwn.h>
#include <winrt/base.h>

#include "BitmapFont.h"
#include "DescriptorHeap.h"
#include "GraphicsDevice.h"
#include "UiDraw.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Neuron
{

class SceneTarget;

// The UI pass (Design/Interface.md §3; TechnicalDesign.md §6.2): the panels, their text and their
// icons drawn over the scene, orthographically at the authored resolution, alpha blended, AFTER the
// fog pass - so that the interface is never darkened by the fog it is telling the player about.
//
// IT DRAWS QUADS AND KNOWS NOTHING ELSE. What a panel looks like is NeuronClient/UiDraw.h's, and it is
// there rather than here because this header includes d3d12.h and that one does not. This file is
// the upload, the pipeline and the draw: a vertex buffer a frame, two atlases uploaded once, one
// root signature, one pipeline state, one DrawInstanced.
//
// ONE DRAW CALL FOR THE WHOLE INTERFACE, AND THEREFORE ONE ORDER. Alpha blending is not
// commutative, so the only thing that makes a panel come out right is that its quads reach the
// rasterizer in the order UiDraw appended them - fill, border, title, widgets. Batching by texture
// would reorder them and paint a button's caption under the button; instead both atlases are bound
// at once and the pixel shader branches on a flat integer the vertex carries. The branch is
// uniform across a quad and costs a comparison.
//
// NO DEPTH, NO MULTISAMPLED RESOLVE OF ITS OWN. The pass binds the scene's colour target alone and
// puts the depth buffer back before it returns; the quads are axis aligned and pixel aligned, so
// multisampling neither helps nor hurts them, and they are drawn into the multisampled target with
// the rest of the frame so that one resolve serves everything.

/// The most quads one frame may hold. §2's six panels with their chrome come to a few hundred, and
/// a full 1920x1080 screen of 16x20 text would be 6,480 glyphs; this is above both with room, and a
/// frame that exceeds it is clamped with a warning rather than allowed to write past the buffer.
inline constexpr std::uint32_t MAX_UI_QUADS = 8192;

class UiPass
{
public:
  /// The atlases are copied into the pass's own textures on the first Draw, because a constructor
  /// has no command list; both are kept decoded until then and their upload buffers afterwards,
  /// which is 360 KB against a deferred-release mechanism for two resources that never change.
  UiPass(GraphicsDevice& _device, const SceneTarget& _scene, const BitmapFont& _font, const IconAtlas& _icons);

  /// THE MINIMAP'S PIXELS FOR THIS FRAME (Design/Interface.md §9.2; m1-vertical-slice/K4), as
  /// MINIMAP_BYTES of RGBA8 from NeuronClient/Minimap.h. Staged here and copied into the texture by the
  /// next Draw, because a copy needs a command list and this does not.
  ///
  /// IT IS A THIRD TEXTURE OF THIS PASS AND NOT A PASS OF ITS OWN. Alpha blending is not
  /// commutative and §3's whole arrangement is that the interface is ONE draw in the order its
  /// quads were appended; a second pass would have to be ordered against this one by hand, and the
  /// first panel drawn over the map would be the one that got it wrong. An empty span leaves the
  /// last frame's map, which is what a frame that had no landscape yet wants.
  void SetMinimap(std::span<const std::uint8_t> _pixels);

  /// Draws _quads over _scene. The colour target is bound on entry and on exit; the depth buffer is
  /// unbound for the draw and bound again before returning, and never transitioned.
  void Draw(ID3D12GraphicsCommandList* _list, const SceneTarget& _scene, std::span<const UiQuad> _quads);

  /// Quads drawn on the last Draw, after any clamp: what a test or a capture reads to tell an empty
  /// interface from one that was refused.
  [[nodiscard]] std::uint32_t LastQuadsDrawn() const noexcept
  {
    return m_lastQuadsDrawn;
  }

private:
  /// Copies both atlases into their textures and leaves them readable. Once, on the first Draw.
  void UploadAtlases(ID3D12GraphicsCommandList* _list);
  /// And the minimap, on every Draw that was given one: to COPY_DEST, copy, back to readable.
  void UploadMinimap(ID3D12GraphicsCommandList* _list);

  GraphicsDevice* m_device;
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipeline;
  winrt::com_ptr<ID3D12Resource> m_fontTexture;
  winrt::com_ptr<ID3D12Resource> m_iconTexture;
  winrt::com_ptr<ID3D12Resource> m_minimapTexture;
  winrt::com_ptr<ID3D12Resource> m_fontUpload;
  winrt::com_ptr<ID3D12Resource> m_iconUpload;
  /// One an in-flight frame, because unlike the two atlases this one is written every frame and a
  /// single buffer would be rewritten while the GPU was still reading it.
  std::array<winrt::com_ptr<ID3D12Resource>, FRAMES_IN_FLIGHT> m_minimapUploads;
  std::array<std::uint8_t*, FRAMES_IN_FLIGHT> m_minimapMapped{};
  std::array<winrt::com_ptr<ID3D12Resource>, FRAMES_IN_FLIGHT> m_vertices;
  std::array<std::uint8_t*, FRAMES_IN_FLIGHT> m_verticesMapped{};
  /// Shader visible, three descriptors in the order UiPS.hlsl declares them: the font, the icons,
  /// then the minimap.
  DescriptorHeap m_views;
  std::vector<UiVertex> m_staging;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_fontFootprint{};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_iconFootprint{};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_minimapFootprint{};
  std::uint32_t m_fontWidth = 0;
  std::uint32_t m_fontHeight = 0;
  std::uint32_t m_iconWidth = 0;
  std::uint32_t m_iconHeight = 0;
  std::uint32_t m_lastQuadsDrawn = 0;
  bool m_atlasesUploaded = false;
  /// Whether the minimap's texture has ever been written. Until it has, it is in COPY_DEST and the
  /// first upload must not transition it FROM the readable state it has never been in.
  bool m_minimapWritten = false;
  /// Which in-flight slot holds this frame's staged pixels, and whether any were staged at all.
  bool m_minimapStaged = false;
};

} // namespace Neuron
