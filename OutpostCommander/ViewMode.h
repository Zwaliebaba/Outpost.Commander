#pragma once

namespace Outpost
{

/// Puts the view into fullscreen, returning whether the request was accepted.
///
/// IT IS ITS OWN TRANSLATION UNIT, and that is a precaution rather than a cure. `App.cpp` already
/// carries the Storage and Display projections on top of this project's precompiled header, and its
/// own comment records that adding Storage to the *pch* failed the build outright with C3859 and
/// C1076. A third projection in that same file is the kind of growth that has broken this project
/// before, so it goes here instead.
///
/// **What it does not fix is C3859 under memory pressure**, which this tree sees intermittently and
/// which has nothing to do with any one file: it was observed striking this file too, whose entire
/// content at the time was `#include "pch.h"`. That failure follows free memory on the machine --
/// an editor and a virtual machine resident alongside the build -- and the answer to it is to free
/// memory or rebuild, never to trim the language standard or reach for `/Zm`.
///
/// **The caller still has to wait for the transition.** This returns as soon as the request is
/// made, and `CoreWindow::Bounds()` reports the old size until the resize has been dispatched.
///
/// **IT IS THE FALLBACK AND NOT THE MECHANISM.** On this shell -- a bare `IFrameworkView` over a
/// `CoreWindow`, no XAML -- this call was observed returning `false` every time, before activation
/// and after it, leaving a 1024 x 768 window. `PreferFullScreenLaunch` below is what actually works
/// and it has to be set BEFORE the view is activated. This stays as the second chance.
///
/// **Do not build a retry loop around it.** Waiting on `CoreWindow::Activated` does not help either
/// -- that event never fired here at all, and a loop pumping the dispatcher while waiting for it
/// cost one run 77 seconds before giving up. See `App.cpp` for what that 77 seconds actually was.
[[nodiscard]] bool TryEnterFullScreen() noexcept;

/// Asks for the view's NEXT activation to be fullscreen, which is the thing that works here.
///
/// **IT MUST BE CALLED BEFORE `CoreWindow::Activate`**, because it is a statement about how the
/// view should come up rather than a request to change one that already has. That is the whole
/// difference between it and `TryEnterFullScreen`, and it is why this one is called from
/// `SetWindow` while that one is called from inside the probe.
///
/// The preference is persisted by the platform per application, so a run that sets it has set it
/// for the next run too. For a gate that is a convenience rather than a hazard: M0.22 owns
/// fullscreen-at-launch as shipped behavior and this is that behavior, made early.
void PreferFullScreenLaunch() noexcept;

/// Whether the view IS fullscreen, as opposed to whether a request to make it so was accepted.
///
/// THE GATE NEEDS THIS ONE AND NOT THE OTHER. `TryEnterFullScreen` returning false on the attempt
/// that found the view already fullscreen is not a failure, and returning true is not yet a panel
/// -- the only thing that makes M0.16's figures the Surface Pro's rather than a window's is the
/// state, checked after the transition has been pumped.
[[nodiscard]] bool IsFullScreenMode() noexcept;

} // namespace Outpost
