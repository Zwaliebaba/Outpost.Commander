#pragma once

#include "NeuronCore.h"

#include "FrameStatistics.h"

#include <cstdint>
#include <memory>

struct IUnknown;

namespace Neuron
{

/// The device, the queue, the allocators and the fences. DECLARED HERE AND DEFINED IN THE .cpp,
/// for the reason DatagramTransport gives about projection headers and for one more: `<d3d12.h>`
/// and `<dxgi1_6.h>` in this library's master include would reach the package's precompiled
/// header, the two DESKTOP test DLLs, and everything else downstream. That chain is already large
/// enough that MSVC has failed to allocate room for it more than once in this tree.
struct DeviceBinding;

enum class DeviceState : std::uint8_t
{
  /// Nothing created yet, or destroyed.
  Absent,
  Ready,
  /// DXGI_ERROR_DEVICE_REMOVED or _RESET. **A PATH TO BE WRITTEN, NOT A CRASH TO BE MET** -- a
  /// driver update, a sleep, or an external GPU going away all produce it, and the frame after is
  /// meant to rebuild rather than to die.
  Removed,
  /// Creation failed outright. LastHresult says why.
  Failed
};

/// Direct3D 12, and only Direct3D 12 (R12). No D3D11, no D3D11On12, no helper layer.
///
/// COM LIFETIMES ARE RAII FROM THE FIRST LINE (R12). Every interface below the binding is held in
/// a `winrt::com_ptr`; WRL's `ComPtr` is not used anywhere in this tree, and a raw `AddRef` and
/// `Release` pair in new code is a defect rather than a style.
///
/// TWO FRAMES IN FLIGHT, each with its own command allocator and its own fence value. One frame
/// means the CPU waits for the GPU every frame and neither is ever busy at once; three costs a
/// frame of latency, which ADR-003's tap-to-visible arithmetic cannot afford.
class GraphicsDevice
{
public:
  static constexpr std::uint32_t FRAMES_IN_FLIGHT = 2;

  GraphicsDevice() noexcept;
  ~GraphicsDevice() noexcept;

  GraphicsDevice(const GraphicsDevice&) = delete;
  GraphicsDevice& operator=(const GraphicsDevice&) = delete;
  GraphicsDevice(GraphicsDevice&&) = delete;
  GraphicsDevice& operator=(GraphicsDevice&&) = delete;

  /// Hardware first, then WARP. The fallback is not politeness: a development machine in a virtual
  /// machine or over a remote session has no hardware adapter, and a client that refuses to start
  /// there is one nobody can debug.
  [[nodiscard]] bool Create() noexcept;

  /// Waits for the GPU to finish everything before releasing anything. Destroying a device with
  /// work in flight is a use-after-free with a driver in the middle of it.
  void Destroy() noexcept;

  [[nodiscard]] DeviceState State() const noexcept;

  /// The HRESULT of the last call that failed, as an integer so this header need not know what an
  /// HRESULT is. Zero when nothing has failed.
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// The frame slot this frame is using: 0 or 1.
  [[nodiscard]] std::uint32_t FrameIndex() const noexcept;

  /// The highest shader model this device supports, as a packed nibble pair: 0x67 is Shader Model
  /// 6.7, 0x60 is 6.0. Zero before Create succeeds.
  ///
  /// IT IS A RUNTIME FACT AND NOT A BUILD-TIME ONE. A pipeline state built from DXIL the driver
  /// cannot accept fails at PSO creation, on the device, in front of a player -- so the model the
  /// shaders are compiled at is a claim about every machine this ships to, and this is how that
  /// claim is checked rather than assumed.
  [[nodiscard]] std::uint32_t HighestShaderModel() const noexcept;

  /// Blocks until the GPU has finished every frame submitted so far.
  void WaitForGpu() noexcept;

  /// How long the GPU spent on the most recently completed frame, in microseconds, from a pair of
  /// timestamps written either side of that frame's command list. Zero until one frame has both
  /// been submitted and been waited for.
  ///
  /// IT IS THE GPU'S TIME AND NOT THE FRAME INTERVAL, which is the distinction the M0.16 gate
  /// turns on. `Present(1, 0)` waits for a vertical blank, so the interval between frames is the
  /// refresh period whatever the renderer costs -- it says whether the budget was held and says
  /// nothing at all about how much of it was left. This is the figure ADR-016 and
  /// `TechnicalDesign.md` section 9.5 are owed.
  [[nodiscard]] std::uint64_t LastFrameGpuMicroseconds() const noexcept;

  /// The same frame in the three spans `TechnicalDesign.md` section 9.6 compares: world, the scaled
  /// present, and the interface. False when that frame did not write both marks below, or the device
  /// serves no timestamps -- a missing figure rather than a wrong one.
  [[nodiscard]] bool LastFrameGpuSplit(GpuFrameSplit& _outSplit) const noexcept;

  /// Two timestamps inside the frame, written by the caller because only the caller knows where its
  /// passes end: after the last draw into the scene target, and after the present blit and before the
  /// interface pass (ADR-011's order). Nothing while no frame is recording.
  void MarkWorldDrawn() noexcept;
  void MarkPresentScaled() noexcept;

  /// Signals the current frame's fence, moves to the next slot, and waits for THAT slot's earlier
  /// work only. That is the whole of what "two frames in flight" means: the CPU is one frame ahead
  /// and never more.
  void PresentedFrame() noexcept;

  /// The command queue and the device, as `IUnknown`, so that SwapChain can ask for what it needs
  /// without this header naming a Direct3D type. Null before Create succeeds.
  [[nodiscard]] ::IUnknown* CommandQueueUnknown() const noexcept;
  [[nodiscard]] ::IUnknown* DeviceUnknown() const noexcept;

  /// Resets this frame's allocator and its command list, ready to record. False when the device is
  /// not Ready.
  [[nodiscard]] bool BeginFrame() noexcept;

  /// Closes the command list and submits it. False when there was nothing open.
  [[nodiscard]] bool EndFrameAndSubmit() noexcept;

  /// The recording command list, as `IUnknown`, for the same reason the two above are. SwapChain
  /// asks for it rather than being handed this object's internals, which is what keeps the binding
  /// private to one translation unit.
  [[nodiscard]] ::IUnknown* CommandListUnknown() const noexcept;

private:
  void WriteMark(std::uint32_t _mark) noexcept;

  std::shared_ptr<DeviceBinding> m_binding;
};

} // namespace Neuron
