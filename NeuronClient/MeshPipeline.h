#pragma once

namespace Neuron
{
class Mesh;
class Renderer;

// Draws meshes flat-lit in one color each (ADR-011): a root signature, a pipeline state, and a constant buffer per frame
// in flight for the camera and the light. It knows no game concept: the caller says where each mesh is and its color.
class MeshPipeline : NonCopyable
{
public:
  // What every mesh in a frame shares. The layout matches cbuffer Frame in Shader/MeshVS.hlsl and MeshPS.hlsl.
  struct FrameConstants
  {
    DirectX::XMFLOAT4X4 viewProjection;
    // A unit vector from the scene toward the light.
    DirectX::XMFLOAT3 directionToLight;
    // The share of an object's color it keeps where the light does not reach it, from 0 to 1.
    float ambient;
  };

  // Builds the root signature and the pipeline state for the renderer's formats. Throws winrt::hresult_error on failure.
  explicit MeshPipeline(Renderer& _renderer);

  // Binds the pipeline to the frame's command list and sets the frame's constants. Draw calls follow.
  void BeginDrawing(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, const FrameConstants& _constants);

  // Draws _mesh placed by _world, which may turn, move and scale uniformly, in a linear color.
  void Draw(ID3D12GraphicsCommandList* _commandList, const Mesh& _mesh, const DirectX::XMFLOAT4X4& _world,
            const DirectX::XMFLOAT4& _color) const;

private:
  // The frame's constants rounded up to the size a constant buffer view needs.
  static constexpr UINT FRAME_CONSTANTS_BYTES =
    (sizeof(FrameConstants) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);

  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  // One slot per frame in flight, mapped for the pipeline's lifetime; the renderer's frame index picks the slot the GPU
  // is not reading.
  winrt::com_ptr<ID3D12Resource> m_frameConstants;
  std::byte* m_mappedFrameConstants = nullptr;
};
} // namespace Neuron