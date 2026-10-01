#pragma once

namespace Neuron
{
class Renderer;

// Draws glows (ADR-019): soft round spots of light that face the camera, added to what is already drawn, so they only
// ever brighten it. They are tested against the scene's depth but write none, so a hull hides the glow behind it while
// glows overlap freely, in any order. A frame's glows are one instanced draw. It knows no game concept: the caller says
// where each glow is, how big it is and its color.
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
    // Unit vectors in the world along the screen's right and its up, which each glow's quad is laid along.
    DirectX::XMFLOAT3 screenRight;
    float unused0;
    DirectX::XMFLOAT3 screenUp;
    float unused1;
  };

  // Glows one frame can draw.
  static constexpr UINT MAX_GLOWS = 4096;

  // Builds the root signature and the pipeline state for the renderer's formats. Throws winrt::hresult_error on failure.
  explicit GlowPipeline(Renderer& _renderer);

  // Draws _glows into the frame's command list. Glows past MAX_GLOWS are dropped.
  void Draw(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, const FrameConstants& _constants, std::span<const Glow> _glows);

private:
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  // One slot of MAX_GLOWS glows per frame in flight, mapped for the pipeline's lifetime.
  winrt::com_ptr<ID3D12Resource> m_instances;
  Glow* m_mappedInstances = nullptr;
};
} // namespace Neuron
