#include "pch.h"

#include "GestureSeam.h"

#include <winrt/Windows.Devices.Input.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>

#include <algorithm>
#include <array>

namespace Neuron
{

namespace
{
// R10 permits a local alias where a using-directive is not allowed, which is everywhere but a
// unit-test framework.
namespace Core = winrt::Windows::UI::Core;
namespace DeviceInput = winrt::Windows::Devices::Input;
namespace Foundation = winrt::Windows::Foundation;
namespace Input = winrt::Windows::UI::Input;

/// More contacts than a digitizer reports at once, so the rejected list never has to grow. A
/// contact that does not fit is simply not tracked, which degrades to "one palm is forwarded"
/// rather than to an allocation inside an event handler that may not throw.
inline constexpr std::size_t TRACKED_CONTACT_CAPACITY = 16;
} // namespace

/// Everything the registrations and the frame thread share, held by one shared_ptr.
///
/// THE HANDLERS HOLD A weak_ptr AND NOT A shared_ptr, for the reason `SocketBinding` gives: the
/// window and the recognizer own the delegates, the delegates would own this, and this owns the
/// recognizer. A strong reference there is a cycle that never releases anything.
///
/// NOTHING IN IT IS ATOMIC AND NOTHING TAKES A LOCK, which is the one way this differs from the
/// transport's binding. A `CoreWindow`'s events are delivered by the dispatcher the frame drains
/// once per frame (R18), so every field below is touched on one thread.
struct GestureBinding
{
  Core::CoreWindow window{nullptr};
  Input::GestureRecognizer recognizer{nullptr};

  winrt::event_token pressedToken{};
  winrt::event_token movedToken{};
  winrt::event_token releasedToken{};
  winrt::event_token captureLostToken{};
  winrt::event_token tappedToken{};
  winrt::event_token holdingToken{};
  winrt::event_token startedToken{};
  winrt::event_token updatedToken{};
  winrt::event_token completedToken{};

  AuthoredSpace space{};

  /// A fixed array rather than a vector, so that nothing an event handler does can allocate and
  /// therefore nothing it does can fail for want of memory.
  std::array<InputEvent, GestureSeam::EVENT_CAPACITY> pending{};
  std::size_t pendingCount = 0;

  /// The contacts whose press was refused as a palm, so that their moves and their release are
  /// refused too.
  std::array<std::uint32_t, TRACKED_CONTACT_CAPACITY> rejectedPointerIds{};
  std::size_t rejectedPointerIdCount = 0;

  std::uint32_t activeContactCount = 0;
  std::uint64_t rejectedContactCount = 0;
  std::uint64_t nonTouchPointerCount = 0;
  std::uint64_t droppedEventCount = 0;
  bool attached = false;
};

namespace
{
[[nodiscard]] bool IsRejectedPointer(const GestureBinding& _binding, std::uint32_t _pointerId) noexcept
{
  const auto last = _binding.rejectedPointerIds.begin() + static_cast<std::ptrdiff_t>(_binding.rejectedPointerIdCount);
  return std::find(_binding.rejectedPointerIds.begin(), last, _pointerId) != last;
}

void RememberRejectedPointer(GestureBinding& _binding, std::uint32_t _pointerId) noexcept
{
  if (_binding.rejectedPointerIdCount < _binding.rejectedPointerIds.size())
  {
    _binding.rejectedPointerIds[_binding.rejectedPointerIdCount] = _pointerId;
    ++_binding.rejectedPointerIdCount;
  }
}

void ForgetRejectedPointer(GestureBinding& _binding, std::uint32_t _pointerId) noexcept
{
  const auto last = _binding.rejectedPointerIds.begin() + static_cast<std::ptrdiff_t>(_binding.rejectedPointerIdCount);
  const auto found = std::find(_binding.rejectedPointerIds.begin(), last, _pointerId);
  if (found != last)
  {
    *found = *(last - 1);
    --_binding.rejectedPointerIdCount;
  }
}

/// **R21'S ONE SITE, AND `Interface.md` SECTION 2'S.** Every pointer this library forwards to the
/// recognizer passes through here, and no other function decides either question.
///
/// The device type is settled on every event, because it cannot change for a pointer and asking is
/// free. THE CONTACT SIZE IS SETTLED ONLY ON THE PRESS and the answer is remembered: a palm whose
/// reported rectangle shrinks for one frame must not be adopted into a gesture already running,
/// which is precisely the lurch this rejection exists to prevent.
[[nodiscard]] bool AcceptPointer(GestureBinding& _binding, const Input::PointerPoint& _point, bool _isDown)
{
  // The one number this file compares against a Windows Runtime enumeration, asserted rather than
  // trusted: `GestureArithmetic.h` states it so that a desktop suite can pin the rule without a
  // projection header, and this is what keeps the two from drifting apart.
  static_assert(static_cast<std::uint32_t>(DeviceInput::PointerDeviceType::Touch) == POINTER_DEVICE_TYPE_TOUCH,
                "POINTER_DEVICE_TYPE_TOUCH no longer matches the projection");

  if (!IsTouchPointer(static_cast<std::uint32_t>(_point.PointerDevice().PointerDeviceType())))
  {
    if (_isDown)
    {
      ++_binding.nonTouchPointerCount;
    }
    return false;
  }

  if (!_isDown)
  {
    return !IsRejectedPointer(_binding, _point.PointerId());
  }

  const Foundation::Rect contact = _point.Properties().ContactRect();
  if (!IsFingertipContact(AuthoredLengthFromDips(_binding.space, contact.Width), AuthoredLengthFromDips(_binding.space, contact.Height)))
  {
    ++_binding.rejectedContactCount;
    RememberRejectedPointer(_binding, _point.PointerId());
    return false;
  }

  return true;
}

void PushEvent(GestureBinding& _binding, const InputEvent& _event) noexcept
{
  if (_binding.pendingCount >= _binding.pending.size())
  {
    ++_binding.droppedEventCount;
    return;
  }

  _binding.pending[_binding.pendingCount] = _event;
  ++_binding.pendingCount;
}

[[nodiscard]] InputEvent MakeManipulationEvent(const GestureBinding& _binding, InputEventKind _kind, const Foundation::Point& _position,
                                               const Input::ManipulationDelta& _cumulative) noexcept
{
  const AuthoredPoint at = AuthoredFromDips(_binding.space, _position.X, _position.Y);
  return InputEvent{.kind = _kind,
                    .contactCount = _binding.activeContactCount,
                    .tapCount = 0,
                    .xAuthoredPixels = at.xPixels,
                    .yAuthoredPixels = at.yPixels,
                    .translationXAuthoredPixels = AuthoredLengthFromDips(_binding.space, _cumulative.Translation.X),
                    .translationYAuthoredPixels = AuthoredLengthFromDips(_binding.space, _cumulative.Translation.Y),
                    .scale = _cumulative.Scale,
                    .rotationDegrees = _cumulative.Rotation};
}

// ---------------------------------------------------------------------------------------------
// The handlers. Each is a free function so that the registration below is one line, and each
// swallows an `hresult_error` for the reason `DatagramTransport.cpp` gives about its own: an
// exception escaping a delegate is a terminate rather than a failure, and there is nothing here
// worth ending the process over -- a pointer event that could not be read is a frame of input, and
// the next one is milliseconds away.
// ---------------------------------------------------------------------------------------------

void OnPointerPressed(const std::weak_ptr<GestureBinding>& _weak, const Core::PointerEventArgs& _args) noexcept
{
  const std::shared_ptr<GestureBinding> held = _weak.lock();
  if (!held)
  {
    return;
  }

  try
  {
    const Input::PointerPoint point = _args.CurrentPoint();
    if (!AcceptPointer(*held, point, true))
    {
      return;
    }

    ++held->activeContactCount;
    held->recognizer.ProcessDownEvent(point);
  }
  catch (const winrt::hresult_error&)
  {
  }
}

void OnPointerMoved(const std::weak_ptr<GestureBinding>& _weak, const Core::PointerEventArgs& _args) noexcept
{
  const std::shared_ptr<GestureBinding> held = _weak.lock();
  if (!held)
  {
    return;
  }

  try
  {
    if (!AcceptPointer(*held, _args.CurrentPoint(), false))
    {
      return;
    }

    // THE INTERMEDIATE POINTS AND NOT JUST THE CURRENT ONE. A digitizer samples faster than the
    // frame does, and a recognizer fed one point per frame sees a coarser path than the finger
    // took -- which reaches the player as a tap slop that triggers late and a pan that lags.
    held->recognizer.ProcessMoveEvents(_args.GetIntermediatePoints());
  }
  catch (const winrt::hresult_error&)
  {
  }
}

void OnPointerReleased(const std::weak_ptr<GestureBinding>& _weak, const Core::PointerEventArgs& _args) noexcept
{
  const std::shared_ptr<GestureBinding> held = _weak.lock();
  if (!held)
  {
    return;
  }

  try
  {
    const Input::PointerPoint point = _args.CurrentPoint();
    const bool accepted = AcceptPointer(*held, point, false);
    ForgetRejectedPointer(*held, point.PointerId());
    if (!accepted)
    {
      return;
    }

    if (held->activeContactCount > 0)
    {
      --held->activeContactCount;
    }
    held->recognizer.ProcessUpEvent(point);
  }
  catch (const winrt::hresult_error&)
  {
  }
}

/// A FOURTH SUBSCRIPTION, WHICH THE PLAN'S THREE DO NOT INCLUDE, and it is here because without it
/// the contact count strands. When the shell takes a contact over -- an edge gesture, a system
/// overlay -- capture is lost and NO `PointerReleased` ARRIVES, so a count that only ever decrements
/// there stays high for the rest of the session and every later manipulation latches a number of
/// fingers that are not on the glass.
///
/// Abandoning the gesture is the conservative answer: a canceled contact is not an up, and
/// `ProcessUpEvent` would tell the recognizer something that did not happen.
void OnPointerCaptureLost(const std::weak_ptr<GestureBinding>& _weak) noexcept
{
  const std::shared_ptr<GestureBinding> held = _weak.lock();
  if (!held)
  {
    return;
  }

  try
  {
    held->recognizer.CompleteGesture();
  }
  catch (const winrt::hresult_error&)
  {
  }

  held->activeContactCount = 0;
  held->rejectedPointerIdCount = 0;
}

void OnTapped(const std::weak_ptr<GestureBinding>& _weak, const Input::TappedEventArgs& _args) noexcept
{
  const std::shared_ptr<GestureBinding> held = _weak.lock();
  if (!held)
  {
    return;
  }

  try
  {
    const Foundation::Point position = _args.Position();
    const AuthoredPoint at = AuthoredFromDips(held->space, position.X, position.Y);

    // ADR-017: the count rides the verb rather than being a fourth one. The first tap has already
    // been delivered as its own event and has already acted, so nothing here waits to find out
    // what a tap is.
    PushEvent(*held, InputEvent{.kind = InputEventKind::Tapped,
                                .contactCount = held->activeContactCount,
                                .tapCount = _args.TapCount(),
                                .xAuthoredPixels = at.xPixels,
                                .yAuthoredPixels = at.yPixels});
  }
  catch (const winrt::hresult_error&)
  {
  }
}

void OnHolding(const std::weak_ptr<GestureBinding>& _weak, const Input::HoldingEventArgs& _args) noexcept
{
  const std::shared_ptr<GestureBinding> held = _weak.lock();
  if (!held)
  {
    return;
  }

  try
  {
    // `Started` only. `Completed` is the finger coming off a hold that already fired and `Canceled`
    // is one that never did, and ADR-018 spends this verb on a recenter -- an action taken once.
    if (_args.HoldingState() != Input::HoldingState::Started)
    {
      return;
    }

    const Foundation::Point position = _args.Position();
    const AuthoredPoint at = AuthoredFromDips(held->space, position.X, position.Y);
    PushEvent(*held, InputEvent{.kind = InputEventKind::Holding,
                                .contactCount = held->activeContactCount,
                                .xAuthoredPixels = at.xPixels,
                                .yAuthoredPixels = at.yPixels});
  }
  catch (const winrt::hresult_error&)
  {
  }
}

/// The three manipulation moments differ only in the type of their arguments, and all three carry
/// `Position` and `Cumulative`. A template rather than three functions, and the arguments are read
/// INSIDE the try rather than at the call site -- a property getter is a call across the Windows
/// Runtime like any other, and one that threw from inside a delegate would terminate.
template <typename Args> void OnManipulation(const std::weak_ptr<GestureBinding>& _weak, InputEventKind _kind, const Args& _args) noexcept
{
  const std::shared_ptr<GestureBinding> held = _weak.lock();
  if (!held)
  {
    return;
  }

  try
  {
    PushEvent(*held, MakeManipulationEvent(*held, _kind, _args.Position(), _args.Cumulative()));
  }
  catch (const winrt::hresult_error&)
  {
  }
}

/// Revokes every registration and returns the binding to its unattached state, keeping the
/// counters so that a caller can still read them afterwards.
///
/// IT IS SHARED BY `Detach` AND BY `Attach`'S FAILURE PATH, because a half-registered seam is the
/// one state neither of them may leave behind. Revoking a token that was never issued finds
/// nothing, which is what makes one function serve both.
void TearDown(GestureBinding& _binding) noexcept
{
  try
  {
    if (_binding.window)
    {
      _binding.window.PointerPressed(_binding.pressedToken);
      _binding.window.PointerMoved(_binding.movedToken);
      _binding.window.PointerReleased(_binding.releasedToken);
      _binding.window.PointerCaptureLost(_binding.captureLostToken);
    }

    if (_binding.recognizer)
    {
      _binding.recognizer.Tapped(_binding.tappedToken);
      _binding.recognizer.Holding(_binding.holdingToken);
      _binding.recognizer.ManipulationStarted(_binding.startedToken);
      _binding.recognizer.ManipulationUpdated(_binding.updatedToken);
      _binding.recognizer.ManipulationCompleted(_binding.completedToken);

      // After the revocations, so that whatever it emits on the way out reaches nothing.
      _binding.recognizer.CompleteGesture();
    }
  }
  catch (const winrt::hresult_error&)
  {
    // A revoke that failed is a registration that is about to be released anyway, and every field
    // below still has to be cleared.
  }

  _binding.pressedToken = {};
  _binding.movedToken = {};
  _binding.releasedToken = {};
  _binding.captureLostToken = {};
  _binding.tappedToken = {};
  _binding.holdingToken = {};
  _binding.startedToken = {};
  _binding.updatedToken = {};
  _binding.completedToken = {};

  _binding.recognizer = nullptr;
  _binding.window = nullptr;
  _binding.pendingCount = 0;
  _binding.rejectedPointerIdCount = 0;
  _binding.activeContactCount = 0;
  _binding.attached = false;
}
} // namespace

GestureSeam::GestureSeam() noexcept
  : m_binding{std::make_shared<GestureBinding>()}
{
}

GestureSeam::~GestureSeam() noexcept
{
  Detach();
}

bool GestureSeam::Attach(::IUnknown* _coreWindow, const AuthoredSpace& _space) noexcept
{
  GestureBinding& binding = *m_binding;
  if (binding.attached || (_coreWindow == nullptr))
  {
    return false;
  }

  Core::CoreWindow window{nullptr};
  if (FAILED(_coreWindow->QueryInterface(winrt::guid_of<Core::CoreWindow>(), winrt::put_abi(window))))
  {
    return false;
  }

  const std::weak_ptr<GestureBinding> weak = m_binding;

  try
  {
    binding.window = window;
    binding.recognizer = Input::GestureRecognizer();
    binding.space = _space;

    // THE VOCABULARY, AND THE WHOLE OF IT (R21, `Interface.md` section 3). Tap is the verb, double
    // tap is ADR-017's count on that same verb rather than a fourth one, hold is ADR-018's
    // recenter, and the three manipulation axes are the camera.
    //
    // THE THREE INERTIA SETTINGS ARE ABSENT AND THAT IS A DECISION (ADR-018): the action
    // immediately after positioning this camera is a precise tap, and momentum fights it. Do not
    // enable them to see what it feels like without changing the ADR.
    //
    // `CrossSlide`, `Drag` and `RightTap` are absent too. A cross-slide costs the one-finger drag
    // its unconditional meaning, a drag is a mouse idiom, and a right tap is a second button --
    // which is one of the four things R21 observes a finger does not have.
    binding.recognizer.GestureSettings(Input::GestureSettings::Tap | Input::GestureSettings::DoubleTap | Input::GestureSettings::Hold |
                                       Input::GestureSettings::ManipulationTranslateX | Input::GestureSettings::ManipulationTranslateY |
                                       Input::GestureSettings::ManipulationScale | Input::GestureSettings::ManipulationRotate);

    binding.pressedToken = binding.window.PointerPressed([weak](const Core::CoreWindow&, const Core::PointerEventArgs& _args) noexcept
                                                         { OnPointerPressed(weak, _args); });
    binding.movedToken = binding.window.PointerMoved([weak](const Core::CoreWindow&, const Core::PointerEventArgs& _args) noexcept
                                                     { OnPointerMoved(weak, _args); });
    binding.releasedToken = binding.window.PointerReleased([weak](const Core::CoreWindow&, const Core::PointerEventArgs& _args) noexcept
                                                           { OnPointerReleased(weak, _args); });
    binding.captureLostToken = binding.window.PointerCaptureLost([weak](const Core::CoreWindow&, const Core::PointerEventArgs&) noexcept
                                                                 { OnPointerCaptureLost(weak); });

    binding.tappedToken = binding.recognizer.Tapped([weak](const Input::GestureRecognizer&, const Input::TappedEventArgs& _args) noexcept
                                                    { OnTapped(weak, _args); });
    binding.holdingToken = binding.recognizer.Holding([weak](const Input::GestureRecognizer&, const Input::HoldingEventArgs& _args) noexcept
                                                      { OnHolding(weak, _args); });

    binding.startedToken = binding.recognizer.ManipulationStarted(
      [weak](const Input::GestureRecognizer&, const Input::ManipulationStartedEventArgs& _args) noexcept
      { OnManipulation(weak, InputEventKind::ManipulationStarted, _args); });
    binding.updatedToken = binding.recognizer.ManipulationUpdated(
      [weak](const Input::GestureRecognizer&, const Input::ManipulationUpdatedEventArgs& _args) noexcept
      { OnManipulation(weak, InputEventKind::ManipulationUpdated, _args); });
    binding.completedToken = binding.recognizer.ManipulationCompleted(
      [weak](const Input::GestureRecognizer&, const Input::ManipulationCompletedEventArgs& _args) noexcept
      { OnManipulation(weak, InputEventKind::ManipulationCompleted, _args); });
  }
  catch (const winrt::hresult_error&)
  {
    TearDown(binding);
    return false;
  }

  binding.attached = true;
  return true;
}

void GestureSeam::Detach() noexcept
{
  GestureBinding& binding = *m_binding;
  if (binding.attached)
  {
    TearDown(binding);
  }
}

bool GestureSeam::IsAttached() const noexcept
{
  return m_binding->attached;
}

void GestureSeam::SetAuthoredSpace(const AuthoredSpace& _space) noexcept
{
  m_binding->space = _space;
}

std::size_t GestureSeam::Drain(std::span<InputEvent> _out) noexcept
{
  GestureBinding& binding = *m_binding;
  const std::size_t taken = std::min(_out.size(), binding.pendingCount);
  std::copy_n(binding.pending.begin(), taken, _out.begin());

  // What did not fit stays, in order, for the next call rather than being lost. A caller draining
  // into a buffer smaller than the seam produced is a caller that will be back next frame.
  const std::size_t remaining = binding.pendingCount - taken;
  std::copy_n(binding.pending.begin() + static_cast<std::ptrdiff_t>(taken), remaining, binding.pending.begin());
  binding.pendingCount = remaining;
  return taken;
}

std::uint64_t GestureSeam::RejectedContactCount() const noexcept
{
  return m_binding->rejectedContactCount;
}

std::uint64_t GestureSeam::NonTouchPointerCount() const noexcept
{
  return m_binding->nonTouchPointerCount;
}

std::uint64_t GestureSeam::DroppedEventCount() const noexcept
{
  return m_binding->droppedEventCount;
}

std::uint32_t GestureSeam::ActiveContactCount() const noexcept
{
  return m_binding->activeContactCount;
}

} // namespace Neuron
