# ADR-034 — A weapon's shot look is presentation data, and the Rail Cannon fires a slug

Status: **accepted** · 2026-10-03

## Context

Task 3.5 drew two kinds of shot: a beam for the Lance, which `CombatEffects.cpp` named by its number in the tuning data, and a tracer for every other weapon. Phase 1 adds the Flak Battery and the Rail Cannon (Phase 1 design §5), and countering the enemy means reading its weapons on sight (MVP §10). The Flak Battery's small, fast hits are a tracer's, and its splash already throws a ring as wide as its 20 m (task 5.3). The Rail Cannon fires one hit of 320 every 6 seconds from 240 m: as a tracer it would look like a slow Mass Driver, and as a beam like a Lance, the weapon it is meant to outrange. A weapon's number written into the client's code is a tuning fact in the wrong place, and the third weapon to need its own look would make it a list.

## Decision

1. **`Models.json` lists each weapon whose shot is not a tracer**, in `shots`, with its look: `{ "weapon": 2, "look": "beam" }`. A weapon not listed fires tracers, so the Mass Driver, the Missile Rack and the Flak Battery are not listed. `ModelCatalog` reads it as `WeaponShot`, refuses a look it does not know and a weapon listed twice, and answers `ShotLookOf`. `CombatEffects` is given the list when it is made, and the code names no weapon.
2. **Three looks:**
   - a **tracer**, as task 3.5 drew it;
   - a **beam**, as task 3.5 drew it, in its shooter's side's color (ADR-028);
   - a **slug**, new for the Rail Cannon: a thin white line, 1.5 m wide, that joins the gun and the target at once and fades where it stands over 0.4 s, with a 7 m flash at both ends. It is white whoever fires it, so that it reads apart from a Lance's beam, which is the side's color, narrows and is gone in 0.25 s.
3. **The Pulse Drive's exhaust is a lime green**, `{0.55, 1.0, 0.3}`, beside the Ion's cyan, the Fusion's magenta and the Constructor's gray, and apart from both sides' blue and orange-red. It is provisional: the owner picks it at the run (task 10.2).

## Consequences

- **The look is unproven**, as ADR-019's was: only the owner's run says whether the slug reads as a heavy hit and the Flak Battery's rings read against the Missile Rack's.
- **Nothing on the server changes.** A shot already says which weapon fired it.
- **`GameAppTests` checks it without a GPU:** each weapon's look from the repository's file, the loader's refusals, the slug's shape and fading, and a weapon not listed firing tracers.

## What this forecloses

- A look with numbers of its own in the data, such as a width or a color per weapon: the looks' numbers stay constants in `CombatEffects.cpp`, as task 3.5's are, until a weapon needs one the three looks do not give.
