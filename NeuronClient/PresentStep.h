#pragma once

#include "FitTransform.h"
#include "GraphicsDevice.h"
#include "SceneTarget.h"
#include "SwapChain.h"

#include <cstdint>
#include <memory>

namespace Neuron
{

/// Declared here and defined in the .cpp, for the reason GraphicsDevice's binding is.
struct PresentStepBinding;

/// R13's present: the scene target fitted into the back buffer with the aspect preserved, centered,
/// letterboxed where it does not match, and sampled by the filter M0.12's world fit chose.
///
/// AT 1:1 WITH ONE SAMPLE THIS IS A PURE COPY AND BUYS NOTHING VISIBLE. That is expected and the
/// indirection stays (ADR-016): a flip-model back buffer cannot be multisampled, so the scene
/// target is the only place four samples can ever live, and turning them on is a constant and a
/// resolve rather than a rewrite of every pass.
///
/// THIS CLASS DOES NO ARITHMETIC. The scale, the fitted rectangle and the filter all arrive in a
/// `FitTransform` that M0.12 computed and M0.12's suite pins -- which is why those tests went in
/// three steps before this code, and why there is nothing here for a test to check that they do not
/// already check better.
class PresentStep
{
public:
  /// Point and bilinear, in one heap, selected per frame by the fit's filter rather than by a
  /// second pipeline state. R13's `None` and `Point` cases are both the point sampler: at exactly
  /// 1:1 a point sample lands on the texel center, so it IS the unfiltered copy.
  static constexpr std::uint32_t SAMPLER_COUNT = 2;

  PresentStep() noexcept;
  ~PresentStep() noexcept;

  PresentStep(const PresentStep&) = delete;
  PresentStep& operator=(const PresentStep&) = delete;
  PresentStep(PresentStep&&) = delete;
  PresentStep& operator=(PresentStep&&) = delete;

  /// The root signature, the two samplers, and the pipeline state built from ADR-012's checked-in
  /// DXIL. The back buffer's format is read off the swap chain rather than restated here, so there
  /// is one place it is decided.
  ///
  /// IT IS ALSO THE MEASUREMENT ADR-012 OWES. Compiling at Shader Model 6.7 and the device
  /// reporting 6.7 are two facts; a driver accepting the blob is a third, and this call is the only
  /// thing that can establish it.
  [[nodiscard]] bool Create(const GraphicsDevice& _device, const SwapChain& _swapChain) noexcept;

  void Destroy() noexcept;

  [[nodiscard]] bool IsReady() const noexcept;

  /// The HRESULT of the last call that failed, or zero.
  [[nodiscard]] std::int32_t LastHresult() const noexcept;

  /// Records the blit into WHATEVER RENDER TARGET IS ALREADY BOUND, which is ADR-011's ordering
  /// constraint rather than an omission: the swap chain binds and clears its back buffer, this draws
  /// into it, the interface pass draws into it after, and the swap chain closes it. Binding it here
  /// would make the second pass rebind it for no reason.
  ///
  /// _fit is THE WORLD FIT (ADR-016) -- `ComputeFit` over the scene target's size against the back
  /// buffer's, never `ComputeInterfaceFit`. Passing the interface's here is the defect ADR-016
  /// exists to prevent, seen from the other side.
  [[nodiscard]] bool Record(const GraphicsDevice& _device, const SceneTarget& _sceneTarget, const FitTransform& _fit) noexcept;

private:
  std::shared_ptr<PresentStepBinding> m_binding;
};

} // namespace Neuron
