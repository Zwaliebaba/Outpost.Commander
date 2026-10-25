# ADR-034 — A weapon's shot look is presentation data, and the Rail Cannon fires a slug

Status: **accepted** · 2026-10-03
Owns: shot-looks, shot-look-data, beam-color

## Context

Countering the enemy means reading its weapons on sight (Game §combat.seen), so a weapon's shot must say which weapon it is and whose. The Flak Battery's small, fast hits are a tracer's, and its splash throws a ring as wide as its splash. The Rail Cannon fires one heavy, slow hit from far off: as a tracer it would look like a slow Mass Driver, and as a beam like a Lance, the weapon it outranges. A weapon's number written into the client's code is a tuning fact in the wrong place, and every weapon that needs its own look would make it a longer list.

## Decision

1. **`Models.json` lists each weapon whose shot is not a tracer**, in `shots`, with its look: `{ "weapon": 2, "look": "beam" }`. A weapon not listed fires tracers, so the Mass Driver, the Missile Rack and the Flak Battery are not listed. `ModelCatalog` reads it as `WeaponShot`, refuses a look it does not know and a weapon listed twice, and answers `ShotLookOf`. `CombatEffects` is given the list when it is made, and the code names no weapon.
2. **The looks:**
   - a **tracer**, a short yellow streak that crosses from the gun to the target, then a spark where it lands;
   - a **beam**, which joins the gun and the target at once and narrows as it fades in 0.25 s, in its shooter's side's color (decision 4);
   - a **slug**, for the Rail Cannon: a thin white line, 1.5 m wide, that joins the gun and the target at once and fades where it stands over 0.4 s, with a 7 m flash at both ends. It is white whoever fires it, so that it reads apart from a Lance's beam.

   Every shot starts with a flash at its gun, and a hit of a weapon with a splash also throws an amber ring that runs out to its splash radius, so the player sees what it reached. The looks' sizes, times and colors are constants in `CombatEffects.cpp`.
3. *(retired)*
4. **A beam is its shooter's side's color, taken 35% of the way to white, and 4 m wide.** `CombatEffects::At` takes a `BeamTint` from `GameClient`, which finds the shooter in the view and lightens its set's color. A shooter the view does not hold gets `BEAM_COLOR`, a pale blue. Tracers, flashes and sparks are yellow, which belongs to no side.

## Consequences

- **The look is the client's.** A shot says which weapon fired it, and nothing on the server knows how it looks.
- **Two sides with the same color would share a beam color.** The sets give every player a color of its own.
- **`GameAppTests` checks it without a GPU:** each weapon's look from the repository's file, the loader's refusals, the slug's shape and fading, and a weapon not listed firing tracers.

## What this forecloses

- A look with numbers of its own in the data, such as a width or a color per weapon: the looks' numbers stay constants in `CombatEffects.cpp` until a weapon needs one the three looks do not give.
