#include "pch.h"

#include "ViewMode.h"

// The one projection this file exists to carry. See the header for why it is not in App.cpp.
#include <winrt/Windows.UI.ViewManagement.h>

namespace Outpost
{

bool TryEnterFullScreen() noexcept
{
  // ORDER MATTERS AND IT COST A RUN: asking before the view is activated is refused, and the window
  // stays at its default 1024 x 768 device-independent pixels. On this panel that is a measurement
  // taken at 2048 x 1536 and written down as the Surface Pro's -- a wrong answer that looks exactly
  // like a right one. The caller activates first.
  //
  // IT COST A SECOND RUN TO LEARN THAT ACTIVATING IS NOT ENOUGH. `CoreWindow::Activate` POSTS the
  // activation; the view is not activated until the dispatcher has dispatched it, so a caller that
  // activates and asks in the next statement is still refused. The caller pumps and retries, and
  // then checks `IsFullScreenMode` rather than this return.
  try
  {
    return winrt::Windows::UI::ViewManagement::ApplicationView::GetForCurrentView().TryEnterFullScreenMode();
  }
  catch (const winrt::hresult_error&)
  {
    // A view that cannot be asked is a view that is not fullscreen, which is a thing to report
    // rather than to die of. `noexcept` here is deliberate: the projection throws and the shell
    // above has no handler.
    return false;
  }
}

void PreferFullScreenLaunch() noexcept
{
  // THE ONE THAT WORKS, and it is a different kind of call: a preference consulted when the view is
  // activated, rather than a request to change a view that already is. Asking after activation was
  // refused every time on this shell -- see the header.
  try
  {
    winrt::Windows::UI::ViewManagement::ApplicationView::PreferredLaunchWindowingMode(
      winrt::Windows::UI::ViewManagement::ApplicationViewWindowingMode::FullScreen);
  }
  catch (const winrt::hresult_error&)
  {
    // Nothing to report and nothing to do: the probe checks `IsFullScreenMode` after the window has
    // settled and says plainly in its log when the answer is no, which is the only place the
    // difference matters.
  }
}

bool IsFullScreenMode() noexcept
{
  try
  {
    return winrt::Windows::UI::ViewManagement::ApplicationView::GetForCurrentView().IsFullScreenMode();
  }
  catch (const winrt::hresult_error&)
  {
    return false;
  }
}

} // namespace Outpost
