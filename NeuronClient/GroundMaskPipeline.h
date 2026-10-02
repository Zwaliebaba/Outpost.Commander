#pragma once

namespace Neuron
{
class Renderer;

// Darkens a square of the ground plane (y = 0) by a grid of shades, one per cell, blended smoothly between the cells'
// centers so that no cell's edge shows. A shade of 0 leaves the scene as it is and 1 makes it black. It is drawn over
// whatever is already drawn, without the depth test, so what stands on the ground darkens with it. The shades go to the
// GPU every frame. It knows no game concept: the caller says where the square is and what each cell's shade is.
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
  };

  // Cells one frame can draw: a grid of 256 by 256.
  static constexpr UINT MAX_CELLS = 256 * 256;

  // Builds the root signature and the pipeline state for the renderer's formats. Throws winrt::hresult_error on failure.
  explicit GroundMaskPipeline(Renderer& _renderer);

  // Draws the mask into the frame's command list. _shades holds cellsPerSide squared shades, row by row along x, rows in
  // order of z. Draws nothing when they do not match, or when there are more than MAX_CELLS.
  void Draw(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, const FrameConstants& _constants, std::span<const float> _shades);

private:
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  // One slot of MAX_CELLS shades per frame in flight, mapped for the pipeline's lifetime.
  winrt::com_ptr<ID3D12Resource> m_shades;
  float* m_mappedShades = nullptr;
};
} // namespace Neuron
