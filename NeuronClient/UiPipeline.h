#pragma once

namespace Neuron
{
class Renderer;

// The interface's fonts and sprites rasterized and packed into one atlas at one scale, and the size in pixels each was
// drawn at, fonts first (ADR-030). Making it needs no device, so it may be made on another thread while the device is
// created (ADR-049).
struct UiAtlas
{
  GlyphAtlas glyphs;
  std::vector<float> pixelSizes;
};

// Rasterizes _fonts and _sprites at _scale pixels to a reference unit. Throws Neuron::Exception when a font is not
// installed, and winrt::hresult_error when DirectWrite fails.
[[nodiscard]] UiAtlas RasterizeUiAtlas(const std::vector<FontDesc>& _fonts, const std::vector<SpriteDesc>& _sprites, float _scale);

// Draws the interface over the scene (ADR-015, ADR-030): solid and hatched rectangles, sprites and lines of text in
// several fonts, as textured quads from one glyph atlas, alpha-blended, with no depth. Everything is in back-buffer
// pixels from the top-left corner; laying out in reference units and scaling (ADR-006) is the caller's. It knows no game
// concept: fonts and sprites are named by their index in the lists it was built with.
class UiPipeline : NonCopyable
{
public:
  // Quads one frame can draw.
  static constexpr UINT MAX_QUADS = 8192;

  // Builds the pipeline for the renderer's formats with _atlas, which RasterizeUiAtlas made of _fonts and _sprites, and
  // uploads it. Throws winrt::hresult_error on failure.
  UiPipeline(Renderer& _renderer, std::vector<FontDesc> _fonts, std::vector<SpriteDesc> _sprites, UiAtlas _atlas);

  // Starts a frame's interface on a back buffer of this size, at _scale pixels to a reference unit. When the scale moves
  // a font or a sprite by a whole pixel, the atlas is rasterized again first; that waits for the GPU, so it belongs to a
  // resize, not to every frame.
  void Begin(UINT _widthPixels, UINT _heightPixels, float _scale);

  void FillRect(float _left, float _top, float _width, float _height, const DirectX::XMFLOAT4& _color);

  // A rectangle of diagonal stripes rising to the right, _stripePixels wide every _periodPixels, laid on the screen's
  // pixels so that neighboring hatched rectangles line up.
  void FillHatched(float _left, float _top, float _width, float _height, const DirectX::XMFLOAT4& _color, float _periodPixels,
                   float _stripePixels);

  // One line of text in font _font, in UTF-8, with its top-left corner here and _trackingPixels more between each two
  // characters; characters the atlas does not hold show as its fallback.
  void DrawText(std::size_t _font, std::string_view _text, float _left, float _top, const DirectX::XMFLOAT4& _color,
                float _trackingPixels = 0.0f);

  // Sprite _sprite drawn into this rectangle, mirrored left to right or top to bottom when asked.
  void DrawSprite(std::size_t _sprite, float _left, float _top, float _width, float _height, const DirectX::XMFLOAT4& _color,
                  bool _mirrorX = false, bool _mirrorY = false);

  [[nodiscard]] float TextWidth(std::size_t _font, std::string_view _text, float _trackingPixels = 0.0f) const noexcept
  {
    return m_atlas.fonts[_font].Width(_text, _trackingPixels);
  }

  [[nodiscard]] float LineHeight(std::size_t _font) const noexcept
  {
    return m_atlas.fonts[_font].lineHeight;
  }

  // The family font _font found among those it named.
  [[nodiscard]] const std::wstring& FamilyOf(std::size_t _font) const noexcept
  {
    return m_atlas.fonts[_font].family;
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

  // Whether the atlas was rasterized at the sizes _scale gives every font and sprite, asked every frame without building
  // the list.
  [[nodiscard]] bool IsRasterizedAt(float _scale) const noexcept;
  // Uploads _atlas and points the pipeline's view at it.
  void UseAtlas(UiAtlas _atlas);
  void AddQuad(float _left, float _top, float _right, float _bottom, float _u0, float _v0, float _u1, float _v1,
               const DirectX::XMFLOAT4& _color);

  Renderer& m_renderer;
  std::vector<FontDesc> m_fonts;
  std::vector<SpriteDesc> m_sprites;
  // The sizes the atlas was rasterized at, as PixelSizes gives them.
  std::vector<float> m_pixelSizes;
  GlyphAtlas m_atlas;
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  // The atlas, and the slot of its view in the renderer's shader-visible heap (ADR-051).
  winrt::com_ptr<ID3D12Resource> m_atlasTexture;
  UINT m_atlasView = 0;
  StaticBuffer m_indexBuffer;
  D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
  // One slot of MAX_QUADS quads per frame in flight, mapped for the pipeline's lifetime.
  winrt::com_ptr<ID3D12Resource> m_vertices;
  Vertex* m_mappedVertices = nullptr;
  std::vector<Vertex> m_frameVertices;
  float m_widthPixels = 0.0f;
  float m_heightPixels = 0.0f;
};
} // namespace Neuron