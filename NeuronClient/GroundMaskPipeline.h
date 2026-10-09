#pragma once

namespace Neuron
{
class Renderer;

// Darkens a square of the ground plane (y = 0) by a grid of shades, one per cell, blended smoothly between the cells'
// centers so that no cell's edge shows. A shade of 0 leaves the scene as it is and 1 makes it black. It is drawn over
// whatever is already drawn, without the depth test, so what stands on the ground darkens with it. The shades are a
// single-channel texture, written only when they change, and another pipeline may sample it too (ADR-052). It knows no
// game concept: the caller says where the square is and what each cell's shade is.
class GroundMaskPipeline : NonCopyable
{
public:
  // What a frame's mask is. The layout matches cbuffer Frame in Shader/GroundMaskVS.hlsl and Shader/GroundMaskPS.hlsl.
  struct FrameConstants
  {
    DirectX::XMFLOAT4X4 viewProjection;
    // The square's corner at its lowest x and z, the side of one cell, and the cells along each side.
    float originXMeters;
    float originZMeters;
    float cellMeters;
    UINT cellsPerSide;
    // How much darker the mask is than its shades. A shade up to kneeShade covers in proportion, up to kneeOpacity at
    // kneeShade; a darker one covers as much as its shade says, and never less than kneeOpacity. So a middle shade can be
    // made darker while the darkest keep theirs. A kneeShade of zero leaves every shade as it is.
    float kneeShade;
    float kneeOpacity;
  };

  // The shades' texture is this many texels a side, so a grid may have at most this many cells a side: 500 of 20 m on the
  // 10 km map (ADR-052). Must match TEXTURE_SIDE in Shader/GroundMaskPS.hlsl.
  static constexpr UINT TEXTURE_SIDE = 512;

  // Builds the root signature and the pipeline state for the renderer's formats, and the shades' texture with its view.
  // Throws winrt::hresult_error on failure.
  explicit GroundMaskPipeline(Renderer& _renderer);

  // Writes _shades, _cellsPerSide squared of them row by row along x, rows in order of z, into the shades' texture by a
  // copy recorded into the frame's command list. They stay there until the next call, so call it only when they change.
  // With _changedRows, a flag for each row, only the flagged rows are written and copied, the rest of the texture keeping
  // what it had; without, every row is (ADR-052). Nothing happens when they are not a square grid of at most TEXTURE_SIDE
  // a side.
  void SetShades(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, std::span<const float> _shades, UINT _cellsPerSide,
                 std::span<const std::uint8_t> _changedRows = {});

  // Draws the mask with the shades last set, when the constants' grid is theirs; nothing before the first SetShades.
  void Draw(ID3D12GraphicsCommandList* _commandList, const FrameConstants& _constants) const;

  // The shades' texture's view in the renderer's shader-visible heap, for another pipeline to sample. Its red channel is
  // the shade, cell (x, z) at texel (x, z), and the texels just past the last cell along each side repeat it, so that
  // sampling at the grid's far edge blends in nothing else.
  [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE ShadesView() const noexcept
  {
    return m_shadesView;
  }

private:
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  // The shades as one byte a texel, in a pixel shader resource state between copies, and its view.
  winrt::com_ptr<ID3D12Resource> m_shades;
  D3D12_GPU_DESCRIPTOR_HANDLE m_shadesView{};
  // One slot of TEXTURE_SIDE squared bytes per frame in flight, mapped for the pipeline's lifetime, which SetShades
  // writes and copies from.
  winrt::com_ptr<ID3D12Resource> m_upload;
  std::uint8_t* m_mappedUpload = nullptr;
  // The cells a side of the shades last set; 0 before the first.
  UINT m_cellsPerSide = 0;
};
} // namespace Neuron
