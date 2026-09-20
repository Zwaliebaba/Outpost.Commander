# ADR-021 — Touch is the only input

**Status:** Accepted; supersedes `Design/Interface.md` §4, §6, §7 and §12 ruling 3 wholesale, and amends [`ADR-013`](ADR-013-uwp-application-model.md) on what device the client runs on. **Two statements in the Context below were corrected on 2026-09-20 by `t1-touch-interface/T1`** — see **Corrections**. The decision itself is unchanged; both corrections are in its favour.
**Date:** 2026-09-20
**Owner:** the owner, 2026-09-20

## Corrections

**Written before any of this had been read against the API reference.** `T1` read it, and two things this ADR asserted turned out to be wrong. Neither changes the decision and both make it cheaper, which is the good way to be wrong; they are recorded here rather than edited into the text above, because an ADR that quietly becomes correct teaches a later reader nothing.

**1. Touch has a second button.** The Context says "Nor is there a second button: every order in `Interface.md` §7 is issued by a right-click or a hotkey, and touch has neither." The first half is false. `GestureSettings::RightTap` is documented as "Touch: press and hold", raising `RightTapped` "when the contact is lifted", and `Holding` fires while the contact is still down — so a finger has a primary and a secondary action, and the pair even comes with a free look-before-you-commit that a mouse button does not have. `Interface.md` §6's default-order table therefore **survives as a table**, with the button replaced by which gesture rather than redesigned around an absence. What is genuinely true is the narrower claim: there is no *modifier* and no *hover*, and press-and-hold costs a dwell, so it belongs on rare and destructive acts rather than on the ones a player does constantly.

**2. `ADR-004` could never have been the answer.** The Consequences hand `T1` the question of whether the interface "fits at the authored resolution or whether `ADR-004`'s 1920×1080 is itself reopened", and `Design/UwpMigration.md` ranked that as risk 3. **Reopening it buys nothing at all.** `T1`'s arithmetic (`Interface.md` §13) reduces the required target to `14400 / width_mm` — the authored resolution cancels out of the formula entirely, because a target and a panel are both measured in authored pixels and both scale with it. The number of buttons that fit on a screen is `width_mm / 7.5` and no choice this tree can make changes it. `ADR-004` stands, untouched, and the risk it was ranked under was a phantom. Both documents are corrected.

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

**Answered by `T1` on 2026-09-20: the touch target is 64 authored pixels**, and it is arithmetic rather than a measurement — stated as such, with the arithmetic shown, in `Interface.md` §13.

The platform figure it rests on is Microsoft's *Guidelines for touch targets*: **7.5 mm square**, "40x40 pixels on a 135 PPI display at a 1.0x scaling plateau", with both of its stated adjustments pointing at *larger* for this game — frequently pressed targets and targets whose mis-press destroys something, of which `Demolish`, `Surrender` and `CancelResearch` are three. Carried through `FitAuthored`, the requirement reduces to `a = 14400 / width_mm`, where `width_mm` is the display's used width; the resolution and the DPI both cancel. 64 authored pixels satisfies 7.5 mm on any display at least 225 mm wide — 9.2 mm on a 13-inch Surface Pro — and falls 0.1 mm short on a 10.5-inch Surface Go, which is stated rather than rounded away.

**The budget that follows is the finding that matters**: `width_mm / 7.5` targets across, which is 29 on the smallest plausible tablet against the 40-odd the mouse interface assumed. §13 lists what fell out to pay for it, and every item is a **count** rather than a size.

**The device is a Surface Pro** (owner, 2026-09-20), which settles the arithmetic and leaves only the judgement. Every Surface Pro ever made clears 225 mm — the narrowest, the 12-inch Pro 3, is 253.6 mm — so **64 stands on any model**, giving 8.45 mm on a 12-inch and 9.16 mm on a 13-inch against the 7.5 mm asked for. That 13 to 22% is spent deliberately on the guidance's own "frequently pressed" clause rather than banked, and dropping to the 53 or 56 the device would allow would buy four columns and cost all of it.

This also pins two numbers `p1-uwp-shell/P9` will want: a 13-inch Pro at 2880×1920 scales the authored frame by exactly **1.5** with 300 physical rows of letterbox, and a 12.3-inch Pro at 2736×1824 by 1.425 with 285. Neither is an integer multiple, so `AGENTS.md` §5's point-sampled path does not run on this device and the bilinear one does.

**Still owed, and it is a judgement rather than a measurement**: whether 64 is enough in a fight rather than at rest, which only the owner's run can say. `t1-touch-interface/T6` records it. The answer if it is not is fewer targets, never smaller ones.

**Not measured and owed by the tasks that create them:** the `GestureSettings` mask the recogniser is configured with, from `p1-uwp-shell/P8`; and the one site that drops a non-touch `PointerPoint`, file and line, from `t1-touch-interface/T5` — so that the reversal this ADR calls cheap is a known deletion rather than a search. Both are written here rather than pinned by a test, because neither is reachable by a suite: a `GestureRecognizer` cannot be constructed outside a package, and that is the honest limit of `AGENTS.md` R20 rather than an exception to it. `T1` has fixed **which** mask (`Interface.md` §4), so what `P8` owes is the confirmation that the tree sets it and nothing else. **Between `P8` and `T5` a mouse will drive the game**, which is deliberate: the shell is debuggable on an ordinary desk for exactly as long as it takes `T5` to land, and no longer.
