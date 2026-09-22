#include "pch.h"

#include "SceneTarget.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <winrt/base.h>

#include <array>

namespace Neuron
{

struct SceneTargetBinding
{
  winrt::com_ptr<ID3D12Resource> color;
  winrt::com_ptr<ID3D12Resource> depth;

  /// Three views of two resources, in three heaps, because a heap holds one type. The first two are
  /// written by the CPU and read by the pipeline; the third is the only one a shader can see.
  winrt::com_ptr<ID3D12DescriptorHeap> renderTargetHeap;
  winrt::com_ptr<ID3D12DescriptorHeap> depthStencilHeap;
  winrt::com_ptr<ID3D12DescriptorHeap> shaderResourceHeap;

  D3D12_GPU_DESCRIPTOR_HANDLE shaderResourceHandle{};

  std::array<float, 4> clearColor{0.0f, 0.0f, 0.0f, 1.0f};
  std::int32_t widthPixels = 0;
  std::int32_t heightPixels = 0;
  HRESULT lastHresult = S_OK;
  bool ready = false;
};

namespace
{
/// The same format the back buffer is, so the present step is a copy rather than a color
/// conversion. Both are UNORM and neither is _SRGB, so at 1:1 the bytes that leave this target are
/// the bytes that reach the glass.
inline constexpr DXGI_FORMAT COLOR_FORMAT = DXGI_FORMAT_B8G8R8A8_UNORM;

/// No stencil, because nothing in this renderer has asked for one. D32 over D24S8 costs nothing
/// here -- both are four bytes a sample.
inline constexpr DXGI_FORMAT DEPTH_FORMAT = DXGI_FORMAT_D32_FLOAT;

/// Far. The depth test is `LESS`, so this is what an empty frame leaves behind.
inline constexpr float DEPTH_CLEAR = 1.0f;

// THE DAY `WORLD_SAMPLE_COUNT` MOVES OFF ONE, THE VIEW BELOW STOPS BEING LEGAL. A multisampled
// texture is a `Texture2DMS` and cannot be filtered by a full-screen blit, so what lands with the
// four samples `TechnicalDesign.md` section 6 asks for is a resolve into a single-sample target
// between the world pass and this one. That is the constant change ADR-016 promised -- a constant
// and a resolve -- and it is asserted here rather than commented, because the alternative failure
// is a picture that nobody is looking at.
static_assert(SceneTarget::SAMPLE_COUNT == 1, "A multisampled scene target needs a resolve before the present step can sample it");

[[nodiscard]] bool OpenCommandList(const GraphicsDevice& _device, winrt::com_ptr<ID3D12GraphicsCommandList>& _outList) noexcept
{
  return (_device.CommandListUnknown() != nullptr) &&
         SUCCEEDED(_device.CommandListUnknown()->QueryInterface(winrt::guid_of<ID3D12GraphicsCommandList>(), _outList.put_void()));
}
} // namespace

SceneTarget::SceneTarget() noexcept
  : m_binding{std::make_shared<SceneTargetBinding>()}
{
}

SceneTarget::~SceneTarget() noexcept
{
  Destroy();
}

bool SceneTarget::Create(const GraphicsDevice& _device, const Desc& _desc) noexcept
{
  SceneTargetBinding& binding = *m_binding;
  binding.ready = false;
  binding.lastHresult = S_OK;

  if ((_desc.widthPixels <= 0) || (_desc.heightPixels <= 0) || (_device.State() != DeviceState::Ready))
  {
    binding.lastHresult = E_INVALIDARG;
    return false;
  }

  winrt::com_ptr<ID3D12Device> device;
  if (FAILED(_device.DeviceUnknown()->QueryInterface(winrt::guid_of<ID3D12Device>(), device.put_void())))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  binding.clearColor = {_desc.clearRed, _desc.clearGreen, _desc.clearBlue, 1.0f};

  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT,
                                             .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                             .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
                                             .CreationNodeMask = 1,
                                             .VisibleNodeMask = 1};

  D3D12_RESOURCE_DESC description{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                  .Alignment = 0,
                                  .Width = static_cast<UINT64>(_desc.widthPixels),
                                  .Height = static_cast<UINT>(_desc.heightPixels),
                                  .DepthOrArraySize = 1,
                                  .MipLevels = 1,
                                  .Format = COLOR_FORMAT,
                                  .SampleDesc = {.Count = SAMPLE_COUNT, .Quality = 0},
                                  .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                  .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET};

  // The union in a clear value is why these are assigned rather than braced.
  D3D12_CLEAR_VALUE clearValue{};
  clearValue.Format = COLOR_FORMAT;
  clearValue.Color[0] = binding.clearColor[0];
  clearValue.Color[1] = binding.clearColor[1];
  clearValue.Color[2] = binding.clearColor[2];
  clearValue.Color[3] = binding.clearColor[3];

  // IT IS CREATED AS A RENDER TARGET AND ENDS EVERY FRAME AS ONE. The present step is the only
  // thing that moves it to a shader resource and it moves it straight back, so there is one state
  // at a frame boundary and no first-frame special case.
  binding.lastHresult =
    device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
                                    winrt::guid_of<ID3D12Resource>(), binding.color.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  description.Format = DEPTH_FORMAT;
  description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_CLEAR_VALUE depthClearValue{};
  depthClearValue.Format = DEPTH_FORMAT;
  depthClearValue.DepthStencil.Depth = DEPTH_CLEAR;
  depthClearValue.DepthStencil.Stencil = 0;

  binding.lastHresult =
    device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue,
                                    winrt::guid_of<ID3D12Resource>(), binding.depth.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  const D3D12_DESCRIPTOR_HEAP_DESC renderTargetHeapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .NumDescriptors = 1, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE, .NodeMask = 0};
  binding.lastHresult =
    device->CreateDescriptorHeap(&renderTargetHeapDescription, winrt::guid_of<ID3D12DescriptorHeap>(), binding.renderTargetHeap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  const D3D12_DESCRIPTOR_HEAP_DESC depthStencilHeapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, .NumDescriptors = 1, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE, .NodeMask = 0};
  binding.lastHresult =
    device->CreateDescriptorHeap(&depthStencilHeapDescription, winrt::guid_of<ID3D12DescriptorHeap>(), binding.depthStencilHeap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  // SHADER VISIBLE, and the only one of the three that is. It is bound alongside the present step's
  // sampler heap, which is a different type, so both fit the one-heap-per-type rule the command
  // list enforces.
  const D3D12_DESCRIPTOR_HEAP_DESC shaderResourceHeapDescription{
    .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .NumDescriptors = 1, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, .NodeMask = 0};
  binding.lastHresult = device->CreateDescriptorHeap(&shaderResourceHeapDescription, winrt::guid_of<ID3D12DescriptorHeap>(),
                                                     binding.shaderResourceHeap.put_void());
  if (FAILED(binding.lastHresult))
  {
    return false;
  }

  device->CreateRenderTargetView(binding.color.get(), nullptr, binding.renderTargetHeap->GetCPUDescriptorHandleForHeapStart());

  D3D12_DEPTH_STENCIL_VIEW_DESC depthViewDescription{};
  depthViewDescription.Format = DEPTH_FORMAT;
  depthViewDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  depthViewDescription.Flags = D3D12_DSV_FLAG_NONE;
  depthViewDescription.Texture2D.MipSlice = 0;
  device->CreateDepthStencilView(binding.depth.get(), &depthViewDescription,
                                 binding.depthStencilHeap->GetCPUDescriptorHandleForHeapStart());

  D3D12_SHADER_RESOURCE_VIEW_DESC shaderViewDescription{};
  shaderViewDescription.Format = COLOR_FORMAT;
  shaderViewDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  shaderViewDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  shaderViewDescription.Texture2D.MostDetailedMip = 0;
  shaderViewDescription.Texture2D.MipLevels = 1;
  shaderViewDescription.Texture2D.PlaneSlice = 0;
  shaderViewDescription.Texture2D.ResourceMinLODClamp = 0.0f;
  device->CreateShaderResourceView(binding.color.get(), &shaderViewDescription,
                                   binding.shaderResourceHeap->GetCPUDescriptorHandleForHeapStart());

  binding.shaderResourceHandle = binding.shaderResourceHeap->GetGPUDescriptorHandleForHeapStart();
  binding.widthPixels = _desc.widthPixels;
  binding.heightPixels = _desc.heightPixels;
  binding.ready = true;
  return true;
}

void SceneTarget::Destroy() noexcept
{
  SceneTargetBinding& binding = *m_binding;
  binding.shaderResourceHeap = nullptr;
  binding.depthStencilHeap = nullptr;
  binding.renderTargetHeap = nullptr;
  binding.depth = nullptr;
  binding.color = nullptr;
  binding.shaderResourceHandle = {};
  binding.ready = false;
  binding.widthPixels = 0;
  binding.heightPixels = 0;
}

bool SceneTarget::IsReady() const noexcept
{
  return m_binding->ready;
}

std::int32_t SceneTarget::WidthPixels() const noexcept
{
  return m_binding->widthPixels;
}

std::int32_t SceneTarget::HeightPixels() const noexcept
{
  return m_binding->heightPixels;
}

std::uint32_t SceneTarget::ColorFormatCode() const noexcept
{
  return static_cast<std::uint32_t>(COLOR_FORMAT);
}

std::uint32_t SceneTarget::DepthFormatCode() const noexcept
{
  return static_cast<std::uint32_t>(DEPTH_FORMAT);
}

std::int32_t SceneTarget::LastHresult() const noexcept
{
  return static_cast<std::int32_t>(m_binding->lastHresult);
}

bool SceneTarget::RecordClear(const GraphicsDevice& _device) noexcept
{
  SceneTargetBinding& binding = *m_binding;
  if (!binding.ready)
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = binding.renderTargetHeap->GetCPUDescriptorHandleForHeapStart();
  const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = binding.depthStencilHeap->GetCPUDescriptorHandleForHeapStart();

  // NO BARRIER HERE. This target is in the render-target state at every frame boundary -- see
  // Create -- because the present step puts it back before the frame ends.
  commandList->OMSetRenderTargets(1, &renderTargetView, FALSE, &depthStencilView);
  commandList->ClearRenderTargetView(renderTargetView, binding.clearColor.data(), 0, nullptr);
  commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, DEPTH_CLEAR, 0, 0, nullptr);
  return true;
}

bool SceneTarget::RecordCalibrationPattern(const GraphicsDevice& _device) noexcept
{
  SceneTargetBinding& binding = *m_binding;
  if (!binding.ready)
  {
    return false;
  }

  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  if (!OpenCommandList(_device, commandList))
  {
    binding.lastHresult = E_NOINTERFACE;
    return false;
  }

  const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = binding.renderTargetHeap->GetCPUDescriptorHandleForHeapStart();

  // Full white, so that anything but full white on the glass is the resample this pattern exists to
  // catch. The optimized clear value does not apply to a cleared RECTANGLE, so there is no fast
  // path being given up here and no debug-layer complaint to answer.
  const std::array<float, 4> white{1.0f, 1.0f, 1.0f, 1.0f};

  // The strips are computed in the header, where a suite can reach them; all that happens here is
  // the copy into the Direct3D type the header may not name.
  const auto strips = CalibrationStrips(binding.widthPixels, binding.heightPixels);

  std::array<D3D12_RECT, CALIBRATION_STRIP_COUNT> rects{};
  UINT count = 0;
  for (const CalibrationStrip& strip : strips)
  {
    if (strip.AreaPixels() > 0)
    {
      rects[count] = D3D12_RECT{.left = static_cast<LONG>(strip.left),
                                .top = static_cast<LONG>(strip.top),
                                .right = static_cast<LONG>(strip.right),
                                .bottom = static_cast<LONG>(strip.bottom)};
      ++count;
    }
  }

  // A target too small to carry the pattern draws none of it rather than clearing the whole thing:
  // ClearRenderTargetView with a count of zero and a null list clears the ENTIRE view, which would
  // paint the frame white and read as a present step that had failed wide open.
  if (count == 0)
  {
    return false;
  }

  commandList->ClearRenderTargetView(renderTargetView, white.data(), count, rects.data());
  return true;
}

::IUnknown* SceneTarget::ColorResourceUnknown() const noexcept
{
  return m_binding->color.get();
}

::IUnknown* SceneTarget::ShaderResourceHeapUnknown() const noexcept
{
  return m_binding->shaderResourceHeap.get();
}

std::uint64_t SceneTarget::ShaderResourceHandle() const noexcept
{
  return m_binding->shaderResourceHandle.ptr;
}

} // namespace Neuron
