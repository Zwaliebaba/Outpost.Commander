#pragma once

namespace Neuron
{
class Renderer;

// Draws the interface over the scene (ADR-015): solid rectangles and lines of text, as textured quads from one glyph
// atlas, alpha-blended, with no depth. Everything is in back-buffer pixels from the top-left corner; laying out in
// reference units and scaling (ADR-006) is the caller's. It knows no game concept.
class UiPipeline : NonCopyable
{
public:
  // Quads one frame can draw.
  static constexpr UINT MAX_QUADS = 8192;

  // Builds the pipeline for the renderer's formats and rasterizes _fontFamily at _fontPixels. Throws
  // winrt::hresult_error on failure.
  UiPipeline(Renderer& _renderer, std::wstring_view _fontFamily, float _fontPixels);

  // Starts a frame's interface on a back buffer of this size. When _fontPixels differs from the atlas's size by a whole
  // pixel, the font is rasterized again first; that waits for the GPU, so it belongs to a resize, not to every frame.
  void Begin(UINT _widthPixels, UINT _heightPixels, float _fontPixels);

  void FillRect(float _left, float _top, float _width, float _height, const DirectX::XMFLOAT4& _color);

  // One line of text with its top-left corner here; characters the atlas does not hold show as its fallback.
  void DrawText(std::string_view _text, float _left, float _top, const DirectX::XMFLOAT4& _color);

  [[nodiscard]] float TextWidth(std::string_view _text) const noexcept
  {
    return m_atlas.Width(_text);
  }

  [[nodiscard]] float LineHeight() const noexcept
  {
    return m_atlas.lineHeight;
  }

  // Draws what Begin collected into the frame's command list. Quads past MAX_QUADS are dropped.
  void End(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex);

private:
  // Matches Shader/UiVS.hlsl's input.
  struct Vertex
  {
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 texel;
    DirectX::XMFLOAT4 color;
  };

  void Rasterize(float _fontPixels);
  void AddQuad(float _left, float _top, float _right, float _bottom, float _u0, float _v0, float _u1, float _v1,
               const DirectX::XMFLOAT4& _color);

  Renderer& m_renderer;
  std::wstring m_fontFamily;
  float m_fontPixels = 0.0f;
  GlyphAtlas m_atlas;
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  winrt::com_ptr<ID3D12DescriptorHeap> m_descriptorHeap;
  winrt::com_ptr<ID3D12Resource> m_atlasTexture;
  winrt::com_ptr<ID3D12Resource> m_indexBuffer;
  D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
  // One slot of MAX_QUADS quads per frame in flight, mapped for the pipeline's lifetime.
  winrt::com_ptr<ID3D12Resource> m_vertices;
  Vertex* m_mappedVertices = nullptr;
  std::vector<Vertex> m_frameVertices;
  float m_widthPixels = 0.0f;
  float m_heightPixels = 0.0f;
};
} // namespace Neuron