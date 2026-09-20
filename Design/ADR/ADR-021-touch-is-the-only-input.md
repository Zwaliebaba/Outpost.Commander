# ADR-021 — Touch is the only input

**Status:** Accepted; supersedes `Design/Interface.md` §4, §6, §7 and §12 ruling 3 wholesale, and amends [`ADR-013`](ADR-013-uwp-application-model.md) on what device the client runs on
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Context

`Design/Interface.md` specifies a mouse-and-keyboard interface in 424 lines and 81 table rows: panel positions in pixels, a table of keys, two pointer modes, and Escape peeled one press at a time. Its §12 ruling 3 (owner, 2026-09-19) made **aim** the default pointer mode — "the mouse drives the camera with no button held, Windows' pointer is hidden, and the ray goes through the screen's **centre**" — and `NeuronClient/PointerMode.h` carries that rule with `NeuronClientTests` pinning the sign of every aim delta, on the stated ground that an inverted camera compiles either way and is noticed by nobody until somebody plays it.

[`ADR-013`](ADR-013-uwp-application-model.md) then made the view a `CoreWindow`, where input arrives as `PointerPoint`s rather than as window messages, and `Windows.UI.Input.GestureRecognizer` is the documented way to turn those into interactions: `ProcessDownEvent`, `ProcessMoveEvents` and `ProcessUpEvent` in, `Tapped`, `Holding` and `ManipulationStarted`/`Updated`/`Completed` out.

**Aim mode has no touch equivalent, and that is the whole of why this is a redesign rather than a port.** There is no hover, no cursor to hide, and no pointer motion without contact. Nor is there a second button: every order in `Interface.md` §7 is issued by a right-click or a hotkey, and touch has neither. The authored resolution of 1920×1080 carries 16-pixel glyphs and 24-pixel buttons, sized for a pointer that is one pixel wide.

## Decision

**Touch is the only input the game takes.** `GestureRecognizer` is the only path from `CoreWindow` into `Neuron::InputQueue`. Keyboard events are not subscribed. Mouse and pen are **not** given a compatibility path: a `PointerPoint` whose `PointerDevice::PointerDeviceType` is not `Touch` is dropped at the seam.

**The client therefore requires a touchscreen, and on a machine without one it does not work at all.** That is a consequence and not an oversight: it was put to the owner on 2026-09-20 against the alternative of one gesture layer fed by every device — which `GestureRecognizer` supports natively, `HoldWithMouse` existing for exactly that — and the owner chose touch alone. It is written here so that nobody later reads the `ADR-013` phrase "desktop, packaged" and concludes the requirement is an accident. **`ADR-013`'s context is amended: the client is a packaged application for a touchscreen device — a tablet, a Surface, or a desktop with a touch panel — and the host is elsewhere on the network.**

**The gesture vocabulary is the design's to fix and this ADR does not fix it.** What this ADR fixes is that it will be expressed in `GestureRecognizer`'s terms and no others: `Tapped`, `Holding`, and the translate, scale and rotate of a manipulation. `t1-touch-interface/T1` writes it, and the owner accepts it by merging, the way `m1-vertical-slice/D1` worked.

**The rule is `AGENTS.md` R21**, and the work is planned in two places on purpose. The *plumbing* — `GestureRecognizer` into `InputQueue`, the arithmetic under a gesture, the one site that drops a non-touch pointer — is [`tasks/p1-uwp-shell.yaml`](../../tasks/p1-uwp-shell.yaml) `P5` and `P8`, because writing a `VirtualKey` path there and deleting it a fortnight later is a fortnight spent twice. The *interface* is [`tasks/t1-touch-interface.yaml`](../../tasks/t1-touch-interface.yaml), six tasks blocked on that shell rather than duplicating it.

**The seam does not change.** A gesture becomes a `Neuron::InputEvent` and goes into the same `InputQueue` a window procedure filled, for the same reason `TechnicalDesign.md` §6.5 gives: a message becomes a record, and the frame decides what it meant. `InputRouter` and `InputSubscription` are unaffected and their suites stand.

**What is deleted, and it is not small.** Aim mode and its half of `PointerMode.h`, with `AIM_RADIANS_PER_COUNT`, `Aim`, `AimedBy` and the tests that pin their signs. `InputEvent`'s `KeyDown`, `KeyUp`, `Character`, `MouseButtonDown`, `MouseButtonUp`, `MouseMove`, `MouseRawMove` and `Wheel`. The key table of `Interface.md` §6. The edge-scrolling of §4. `EscapePeel` survives as a *concept* — the innermost thing is cancelled first — but Escape is not a key any more and the gesture that peels is `T1`'s to choose.

## Consequences

**A developer without a touchscreen cannot run the game.** The answers are the Visual Studio **Simulator**, which injects touch into a deployed package, and `InjectTouchInput` for a scripted harness. `t1-touch-interface/T1` says which is used and the plan's runs name it, because "run it and look at it" (`AGENTS.md` §3) is otherwise unsatisfiable for anyone whose desk has a mouse.

**The CI capture gate is untouched**, and this is the second time `ADR-014`'s boundary has paid for itself: `OutpostCapture` drives a scripted match with no window and no input at all, so the four assertions that stand between this tree and a green build over a broken game do not know that the interface changed.

**`m1-vertical-slice/G3` closes M1 on an interface this ADR replaces.** That is not a contradiction and G3 is not rewritten: M1 proves the *simulation and the replication* over a slice, and it did so through the interface it had. The touch interface is proved by `t1-touch-interface`'s own closing run. G3's notes say so.

**Text has no keyboard.** `Interface.md` §10's chat line takes characters and Enter. A `CoreWindow` app reaches the soft keyboard through `CoreTextEditContext` and `InputPane`, which is real work for one feature that exists, in M1, "to prove the order kind travels rather than to be used". Whether chat survives at all is `T1`'s to decide.

**What would reopen it.** A device target that is not a touchscreen — which would make the refusal of mouse and pen pure cost — or a finding in `T1` that some part of the game cannot be driven by fingers at all. The cheap reversal is already built in: the seam drops non-touch `PointerPoint`s in one place, so admitting mouse and pen later is a deletion rather than a design.

## Measurements

**Counted on this tree at `79ce424`; nothing here is a measurement of a build.**

`Design/Interface.md` is 424 lines and 81 table rows. `NeuronClient/InputEvent.h` is 61 lines and names 9 event kinds, of which **8 are keyboard or mouse** and only `FocusLost` survives untouched. `NeuronClient/PointerMode.h` is 87 lines, of which the aim half and its constant go. `Tests/NeuronClientTests/PointerModeTests.cpp` pins the deltas that go with them.

**Not measured and owed by the tasks that create them:** the `GestureSettings` mask the recogniser is configured with, from `p1-uwp-shell/P8`; and the one site that drops a non-touch `PointerPoint`, file and line, from `t1-touch-interface/T5` — so that the reversal this ADR calls cheap is a known deletion rather than a search. Both are written here rather than pinned by a test, because neither is reachable by a suite: a `GestureRecognizer` cannot be constructed outside a package, and that is the honest limit of `AGENTS.md` R20 rather than an exception to it. **Between `P8` and `T5` a mouse will drive the game**, which is deliberate: the shell is debuggable on an ordinary desk for exactly as long as it takes `T5` to land, and no longer.

**Not measured and owed by `T1`:** the touch target size the authored 1920×1080 can carry, against the 16-pixel glyph and 24-pixel button `Interface.md` §2 specifies now. That number decides whether the interface fits at the authored resolution or whether `ADR-004`'s 1920×1080 is itself reopened, and it is the figure the design task must produce before any panel is redrawn.
