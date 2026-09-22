#pragma once

#include "GestureArithmetic.h"
#include "InputEvent.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

struct IUnknown;

namespace Neuron
{

/// The recognizer, the window and the event registrations. DECLARED HERE AND DEFINED IN THE .cpp,
/// for the reason `DatagramTransport`'s binding is: no C++/WinRT projection header may reach this
/// library's master include, and therefore none may reach `GameClient`, the package, or the two
/// DESKTOP test DLLs that link this library's Windows Store build.
struct GestureBinding;

/// R21's one path from the `CoreWindow` into the game, and there is no other.
///
/// `PointerPressed`, `PointerMoved` and `PointerReleased` are forwarded to a
/// `Windows::UI::Input::GestureRecognizer`; keyboard events are not subscribed at all; and a
/// `PointerPoint` whose `PointerDeviceType` is not `Touch` is dropped at exactly one site. What
/// comes out is an `InputEvent` -- a plain record in authored coordinates -- and nothing else.
///
/// IT IS THE HALF NO SUITE CAN REACH, and that is why it is this small. Nothing in this tree can
/// construct a `CoreWindow` (`Plan/README.md` F6), so every number under a gesture lives in
/// `GestureArithmetic` where a desktop test DLL can get at it, and this class does no arithmetic
/// beyond one coordinate conversion. R21 is explicit about why: the sign of a pinch is a thing a
/// package can hide and a test cannot.
///
/// THREE THINGS HAPPEN HERE AND NOWHERE ELSE (`Interface.md` sections 1 and 2):
///
/// - **The non-touch drop.** One site, and a `static_assert` at it keeps the integer this compares
///   against the projection's rather than a number somebody typed.
/// - **Palm rejection.** A contact whose `ContactRect` exceeds 78 authored pixels in either
///   dimension never becomes an input record. The contact-count latch protects a gesture already
///   running; a palm landing FIRST starts one of its own, and on a 287 mm screen played on a desk
///   that happens routinely. The decision is taken on the press and the contact is then ignored
///   for the rest of its life, because a palm that momentarily reports a smaller rectangle must
///   not be adopted halfway through.
/// - **The contact count**, because the recognizer does not report one.
///
/// THE INERTIA GESTURE SETTINGS STAY OFF (ADR-018). The action immediately after positioning this
/// camera is a precise tap and momentum fights it, so leaving them off is a decision rather than an
/// omission: do not enable them to see what it feels like without changing the ADR.
///
/// IT DOES NOT MARK AN EVENT HANDLED. Whether a fullscreen touch-only client should swallow the
/// shell's edge gestures is a decision nothing in the design has taken, and taking it silently here
/// is the kind of thing nobody finds afterwards.
class GestureSeam
{
public:
  /// How many records one frame may hold before the oldest stop being made. Sixty-four is far more
  /// than a dispatcher drain can produce between two frames at any rate this panel runs at; it
  /// exists so that a frame which stalls cannot grow this without bound, and `DroppedEventCount`
  /// is how that shows up rather than as a mystery.
  static constexpr std::size_t EVENT_CAPACITY = 64;

  GestureSeam() noexcept;
  ~GestureSeam() noexcept;

  GestureSeam(const GestureSeam&) = delete;
  GestureSeam& operator=(const GestureSeam&) = delete;
  GestureSeam(GestureSeam&&) = delete;
  GestureSeam& operator=(GestureSeam&&) = delete;

  /// _coreWindow is the `CoreWindow` as an `IUnknown`, so this header need not name a Windows
  /// Runtime type -- the same arrangement `SwapChain::Create` has.
  ///
  /// EVERYTHING HERE RUNS ON THE FRAME'S THREAD. A `CoreWindow`'s pointer events are delivered by
  /// the dispatcher this application drains once per frame (R18), so unlike `DatagramTransport`
  /// there is no thread to cross and nothing here takes a lock.
  [[nodiscard]] bool Attach(::IUnknown* _coreWindow, const AuthoredSpace& _space) noexcept;

  /// Revokes every registration and completes any gesture in progress. Safe to call twice.
  void Detach() noexcept;

  [[nodiscard]] bool IsAttached() const noexcept;

  /// A window that changed size changed the interface fit, and every conversion after this call
  /// uses the new one. A manipulation in flight keeps its gate, which is `GestureArithmetic`'s, so
  /// a resize mid-drag moves the ground under the finger rather than ending the gesture -- and a
  /// resize mid-drag is not reachable in a client that launches fullscreen (Q23).
  void SetAuthoredSpace(const AuthoredSpace& _space) noexcept;

  /// Copies out what arrived since the last call and empties the seam. Returns how many records
  /// were written, which is never more than _out holds -- anything that does not fit stays for the
  /// next call rather than being lost.
  [[nodiscard]] std::size_t Drain(std::span<InputEvent> _out) noexcept;

  /// Contacts refused as palms (`Interface.md` section 2).
  [[nodiscard]] std::uint64_t RejectedContactCount() const noexcept;

  /// Pointers refused for not being touch (R21). On a machine with a mouse this counts every press
  /// of it, which is the intended behavior and a useful thing to see in a log.
  [[nodiscard]] std::uint64_t NonTouchPointerCount() const noexcept;

  /// Records the seam had nowhere to put. Nonzero means a frame stalled long enough to matter.
  [[nodiscard]] std::uint64_t DroppedEventCount() const noexcept;

  /// Touch contacts down right now, which is what stamps each record. It is the LIVE count; the
  /// latched one a manipulation keeps is `ManipulationGate`'s.
  [[nodiscard]] std::uint32_t ActiveContactCount() const noexcept;

private:
  std::shared_ptr<GestureBinding> m_binding;
};

} // namespace Neuron
