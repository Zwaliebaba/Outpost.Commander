#pragma once

namespace Neuron
{
class Renderer;
struct TextureData;

// Draws glows (ADR-019): spots of light that face the camera, added to what is already drawn, so they only ever brighten
// it. They are tested against the scene's depth but write none, so a hull hides the glow behind it while glows overlap
// freely, in any order. A frame's glows are one instanced draw. Without a sprite, a glow is a soft round spot. With one,
// it is the sprite's color times the glow's, read texel by texel with no smoothing when magnified, as a particle of
// DeepSpaceOutpost's is (ADR-026). It knows no game concept: the caller says where each glow is, how big it is and its
// color.
class GlowPipeline : NonCopyable
{
public:
  // One glow. The layout matches the per-instance input of Shader/GlowVS.hlsl.
  struct Glow
  {
    DirectX::XMFLOAT3 position;
    float radiusMeters;
    // A linear color, its brightness included: what the glow adds to the scene at its center.
    DirectX::XMFLOAT4 color;
  };

  // What a frame's glows share. The layout matches cbuffer Frame in Shader/GlowVS.hlsl.
  struct FrameConstants
  {
    DirectX::XMFLOAT4X4 viewProjection;
    // Vectors in the world along which each glow's quad is laid, its corners at plus and minus each, times its radius:
    // unit vectors along the screen's right and its up for an upright square. Others turn or stretch it.
    DirectX::XMFLOAT3 screenRight;
    float unused0;
    DirectX::XMFLOAT3 screenUp;
    float unused1;
  };

  // Glows one frame can draw.
  static constexpr UINT MAX_GLOWS = 4096;

  // Builds the root signature and the pipeline state for the renderer's formats, and uploads _sprite when there is one,
  // which must have its mip levels. Throws winrt::hresult_error on failure.
  explicit GlowPipeline(Renderer& _renderer, const TextureData* _sprite = nullptr);

  // Draws _glows into the frame's command list. Glows past MAX_GLOWS are dropped.
  void Draw(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, const FrameConstants& _constants, std::span<const Glow> _glows);

private:
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  // One slot of MAX_GLOWS glows per frame in flight, mapped for the pipeline's lifetime.
  winrt::com_ptr<ID3D12Resource> m_instances;
  Glow* m_mappedInstances = nullptr;
  // The sprite, and its view in the renderer's shader-visible heap (ADR-051); empty and null for soft spots.
  winrt::com_ptr<ID3D12Resource> m_sprite;
  D3D12_GPU_DESCRIPTOR_HANDLE m_spriteView{};
};
} // namespace Neuron
