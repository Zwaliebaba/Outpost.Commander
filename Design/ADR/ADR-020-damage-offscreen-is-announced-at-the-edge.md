# ADR-020 — Damage you cannot see is announced at the screen edge

**Status:** Accepted
**Date:** 2026-09-21
**Owner:** Stefan Zwaal — on a review of what the MVP's interface actually covers

## Context

**Nothing in this design tells a player they are under attack.** A review of the MVP's interface surface
found no alert concept anywhere in `Design/`, and three things that together make the omission serious:

- **The hull bar exists only for the *selected* ships** (`Interface.md` §6). Miners dying at the far side
  of the map produce no signal of any kind.
- **There is no audio**, and there will not be for a while — `GameDesign.md` §10 defers it explicitly. The
  channel every other game uses for this is closed.
- **There is no minimap** (`Interface.md` §5), no fog to narrow where attention should go, and the play
  area is 16,384 units on a side.

`GameDesign.md` §7 already states the consequence and reads, in hindsight, like a bug report:
**"a defender must be watching the right part of a 16,384-unit map at the right moment to have any
counterplay."** Without a signal that is not *hard*, it is impossible — the defender is not being asked to
react quickly, they are being asked to guess.

[`ADR-018`](ADR-018-the-camera-is-anchored-to-the-plane.md) added a recenter gesture, which is half an
answer: it makes getting somewhere cheap once you know where. This is the other half.

## Decision

**1. The client derives it from what it is already sent. No wire bytes.** A fire event names shooter,
target and weapon ([`ADR-004`](ADR-004-weapons-resolve-at-the-fire-tick.md)); the replica store holds
positions; the removal list names the dead, whose last position the client had a snapshot ago. **An event
whose target is yours is an attack on you, and the client can see that without being told.** Nothing about
this decision reaches the host or the datagram.

**2. It shows at the screen edge, in the direction of the event, and fades over a few seconds.** Peripheral
rather than central: it is a thing to notice, not a thing to read. **If the event is already on screen
there is no indicator** — you can see it.

**3. It is a touch target, and that is what keeps it free.** Tapping the indicator recenters the camera
there. It is a rectangle the hit test knows about, exactly like a build button — **so it costs no gesture
and R21's budget is untouched.** The banked `Holding` stays banked. This is also the reason the interface
now takes precedence in `Interface.md` §1's pick order, which previously listed only world entities and
never said what happens when a tap lands on both.

It is sized to the **64-pixel combat tier** (`Interface.md` §1): it is hit while something is exploding.

**4. One indicator per cluster, with a count.** A fleet caught in the open is one event to the player, not
forty. Cluster by proximity and by time.

**5. There is no audio, and this is why the visual has to carry it alone.** When audio arrives, a sound is
the natural companion and this decision does not change; it simply stops being the only channel.

## Consequences

**Defense becomes possible, which it was not.** That is the whole of why this is in the MVP rather than
after it: without it, half of what `GameDesign.md` §7 describes cannot be played.

**It tells you where, not what.** The indicator carries a direction and a count and nothing else — not the
attacker's design, not how many, not whether you are losing. Tapping it takes you there, and looking is
how you find out. That is deliberate: a richer alert is a panel, and a panel is screen space this design
does not have.

**It costs a few quads and one hit-test rectangle**, which against a frame that draws 110 entities is not
worth measuring.

**The attention economy is the real cost, and it is the thing to watch.** An indicator that fires too
eagerly becomes wallpaper and then it is worse than nothing, because the player learns to ignore the one
channel they have. Clustering and the on-screen suppression exist for that, and M3 is where it is judged.

## Measurements

None yet. Two are owed at **M3.11**, and both are played rather than computed:

1. **Whether it fires too often to be worth reading.** The failure is habituation, and the fix is
   clustering thresholds rather than a different design.
2. **Whether tapping it is the action a player actually takes**, or whether they ignore it and carry on.
   If the second, the indicator is the wrong shape and a panel entry is the alternative.
