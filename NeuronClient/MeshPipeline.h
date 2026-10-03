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
    // Where the camera is, in the world, toward which DrawLines pulls a line.
    DirectX::XMFLOAT3 eyePosition;
    float unused0;
  };

  // Builds the root signature and the pipeline state for the renderer's formats. Throws winrt::hresult_error on failure.
  explicit MeshPipeline(Renderer& _renderer);

  // Binds the pipeline to the frame's command list and sets the frame's constants. Draw calls follow.
  void BeginDrawing(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex, const FrameConstants& _constants);

  // Draws _mesh placed by _world, which may turn, move and scale uniformly, in a linear color.
  void Draw(ID3D12GraphicsCommandList* _commandList, const Mesh& _mesh, const DirectX::XMFLOAT4X4& _world,
            const DirectX::XMFLOAT4& _color) const;

  // Vertices one frame's DrawTriangles calls can take between them.
  static constexpr UINT MAX_FRAME_VERTICES = 65536;

  // Draws a triangle list made on the CPU this frame, already in the world, in a linear color: geometry that changes every
  // frame, such as an explosion's shards (ADR-026). The vertices are copied into this frame's slot of an upload buffer,
  // so they need not outlive the call. A call that would take the frame past MAX_FRAME_VERTICES draws nothing and returns
  // false. Only after BeginDrawing.
  bool DrawTriangles(ID3D12GraphicsCommandList* _commandList, std::span<const MeshVertex> _vertices, const DirectX::XMFLOAT4& _color);

  // Draws _lines, a line list such as BuildCreaseLines makes, placed by _world, as one-pixel lines lit as a mesh is, in a
  // linear color. They are tested against the depth of what is drawn but write none, so the far side of a mesh hides its
  // own lines. Each vertex is pulled _liftShare of its distance toward the eye, which moves it nowhere on the screen but
  // puts it in front of the surface it lies on: Direct3D gives a line no depth bias (ADR-040). The pipeline is back on
  // Draw's state and topology when it returns.
  void DrawLines(ID3D12GraphicsCommandList* _commandList, const Mesh& _lines, const DirectX::XMFLOAT4X4& _world,
                 const DirectX::XMFLOAT4& _color, float _liftShare) const;

  // One line list to draw, as DrawLines takes it.
  struct LineDraw
  {
    const Mesh* lines = nullptr;
    DirectX::XMFLOAT4X4 world{};
    DirectX::XMFLOAT4 color{};
    float liftShare = 0.0f;
  };

  // Draws every one of _draws as DrawLines does, switching to the line state and back once for all of them.
  void DrawLines(ID3D12GraphicsCommandList* _commandList, std::span<const LineDraw> _draws) const;

private:
  void DrawObject(ID3D12GraphicsCommandList* _commandList, const Mesh& _mesh, const DirectX::XMFLOAT4X4& _world,
                  const DirectX::XMFLOAT4& _color, float _liftShare) const;

  // The frame's constants rounded up to the size a constant buffer view needs.
  static constexpr UINT FRAME_CONSTANTS_BYTES =
    (sizeof(FrameConstants) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);

  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  // The same, drawing lines (DrawLines).
  winrt::com_ptr<ID3D12PipelineState> m_lineState;
  // One slot per frame in flight, mapped for the pipeline's lifetime; the renderer's frame index picks the slot the GPU
  // is not reading.
  winrt::com_ptr<ID3D12Resource> m_frameConstants;
  std::byte* m_mappedFrameConstants = nullptr;
  // One slot of MAX_FRAME_VERTICES vertices per frame in flight for DrawTriangles, mapped for the pipeline's lifetime, and
  // how much of the frame's slot is taken.
  winrt::com_ptr<ID3D12Resource> m_frameVertices;
  MeshVertex* m_mappedFrameVertices = nullptr;
  UINT m_frameIndex = 0;
  UINT m_frameVerticesUsed = 0;
};
} // namespace Neuron