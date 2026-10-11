#pragma once

namespace Neuron
{
class Renderer;

// How the scene glows (ADR-091). Light brighter than thresholdLuminance glows, eased in over kneeLuminance either side of
// it so that nothing starts to glow at a hard edge. intensity is how much of that light is added back, spread over the
// screen. spread, from 0 to 1, weighs each wider level against the one before it: low keeps the glow tight around what is
// bright, as a vector screen's phosphor glows, and high lets it reach far. An intensity of 0 adds nothing.
struct BloomDesc
{
  float thresholdLuminance = 0.0f;
  float kneeLuminance = 0.0f;
  float intensity = 0.0f;
  float spread = 0.0f;
};

// The scene's glow (ADR-091): what is brighter than a threshold, at half the screen's resolution, blurred over LEVEL_COUNT
// levels, each half the size of the one before, and added back up the chain. The renderer's resolve adds the first level to
// the scene (ADR-050), and the interface is drawn after it, so it never glows. The levels are textures of its own in
// FORMAT, which rest as pixel shader resources between frames; their views are in the renderer's shader-visible heap
// (ADR-051). It knows no game concept.
class Bloom : NonCopyable
{
public:
  static constexpr UINT LEVEL_COUNT = 5;
  // Float, so that light added up the chain neither bands nor clips; render targets and blending in it are guaranteed at
  // feature level 11_0 (ADR-006).
  static constexpr DXGI_FORMAT FORMAT = DXGI_FORMAT_R11G11B10_FLOAT;

  // The size of a level, in pixels.
  struct Extent
  {
    UINT widthPixels = 0;
    UINT heightPixels = 0;
  };

  // The levels' sizes for a scene of _widthPixels by _heightPixels: half of it, rounded down, then half of each level in
  // turn, never less than one pixel. Each pixel of the first level covers two by two of the scene's.
  [[nodiscard]] static std::array<Extent, LEVEL_COUNT> LevelExtents(UINT _widthPixels, UINT _heightPixels) noexcept;

  // How much of the first level the resolve adds to the scene: the intensity over the sum of the levels' weights, so that
  // intensity is the share of the bright light added back however far it spreads.
  [[nodiscard]] static float CompositeScale(const BloomDesc& _desc) noexcept;

  // Builds the root signature and the pipeline states, and takes a slot in the renderer's shader-visible heap for each
  // level. The levels themselves are made by Resize. Throws winrt::hresult_error on failure.
  explicit Bloom(Renderer& _renderer);

  // Makes the levels for a scene of this size and writes their views. The GPU must be done with the old ones.
  void Resize(UINT _widthPixels, UINT _heightPixels);

  // Records the glow of the scene whose multisampled view is _sceneView, which must be a pixel shader resource: the bright
  // light into the first level, each level from the one above it, then each back up into the one above. It sets its own
  // root signature, pipeline states, render targets, viewports and scissors, which the caller sets again after it.
  void Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_DESCRIPTOR_HANDLE _sceneView, const BloomDesc& _desc) const;

  // The first level, which holds the whole glow once Draw has run, as the resolve reads it.
  [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE FirstLevelView() const noexcept;
  [[nodiscard]] Extent FirstLevelExtent() const noexcept
  {
    return m_extents[0];
  }

private:
  [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE LevelTargetView(UINT _level) const noexcept;

  Renderer& m_renderer;
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  // Keeps the scene's bright light, halves a level, and adds a level into the one above it.
  winrt::com_ptr<ID3D12PipelineState> m_brightState;
  winrt::com_ptr<ID3D12PipelineState> m_downState;
  winrt::com_ptr<ID3D12PipelineState> m_upState;
  // Each level's render target view, outside the shader-visible heap.
  winrt::com_ptr<ID3D12DescriptorHeap> m_targetHeap;
  UINT m_targetViewBytes = 0;
  std::array<winrt::com_ptr<ID3D12Resource>, LEVEL_COUNT> m_levels;
  std::array<UINT, LEVEL_COUNT> m_levelViews{};
  std::array<Extent, LEVEL_COUNT> m_extents{};
};
} // namespace Neuron
