#pragma once

namespace Neuron
{
class Renderer;
struct TextureData;

// Draws a sky of stars at infinity (ADR-021, ADR-022). Each star is a direction, so moving the camera never moves it and
// only turning the camera does. A star is a square facing the screen, sized in pixels rather than meters, and added to
// the frame, so stars that overlap add up. Its Shape says what it looks like. The sky is drawn first, with no depth test, and everything drawn after it covers it.
// The stars are uploaded once, and a frame draws them all in one instanced draw. It knows no game concept: the caller
// says where each star is, how big and what color.
class StarPipeline : NonCopyable
{
public:
  // What a star looks like in its square.
  enum class Shape : std::uint8_t
  {
    // A Gaussian spot that fades to nothing at the square's inscribed circle.
    Spot,
    // The sprite's alpha in the star's color, upright on the screen, as a camera's diffraction spikes are.
    Sprite,
    // A cross of two lines a pixel wide along the screen's axes, fading from the center to the square's edge, with a
    // dot at the center: a vector-drawn star (ADR-028).
    Cross
  };

  // One star. The layout matches the per-instance input of Shader/StarVS.hlsl.
  struct Star
  {
    // A unit vector in the world, toward the star.
    DirectX::XMFLOAT3 direction;
    // Half the square's side, in pixels at the scale FrameConstants gives.
    float radiusPixels;
    // A linear color, its brightness included: what the star adds to the frame where it is brightest.
    DirectX::XMFLOAT3 color;
  };

  // What a frame's stars share. The layout matches cbuffer Frame in Shader/StarVS.hlsl.
  struct FrameConstants
  {
    // The camera's view and projection. Only its rotation reaches a star, since a direction has no position.
    DirectX::XMFLOAT4X4 viewProjection;
    // How far one pixel of a star's radius is in clip space across the screen and up it: 2 / width and 2 / height for
    // stars sized in the back buffer's pixels, or scaled by the caller.
    float clipPerPixelX;
    float clipPerPixelY;
    float unused0;
    float unused1;
  };

  // Builds the root signature and the pipeline state for the renderer's formats, and uploads _stars, which the sky then
  // keeps, drawn as _shape, and _sprite, which Shape::Sprite needs and the others ignore, and which must have its mip
  // levels. Throws winrt::hresult_error on failure, and when Shape::Sprite has no sprite.
  StarPipeline(Renderer& _renderer, std::span<const Star> _stars, Shape _shape = Shape::Spot, const TextureData* _sprite = nullptr);

  // Draws every star into the frame's command list. Call it before anything else is drawn.
  void Draw(ID3D12GraphicsCommandList* _commandList, const FrameConstants& _constants) const;

private:
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  StaticBuffer m_instances;
  // The sprite, and its view in the renderer's shader-visible heap (ADR-051); empty and null for Gaussian stars.
  winrt::com_ptr<ID3D12Resource> m_sprite;
  D3D12_GPU_DESCRIPTOR_HANDLE m_spriteView{};
  UINT m_starCount = 0;
};
} // namespace Neuron
