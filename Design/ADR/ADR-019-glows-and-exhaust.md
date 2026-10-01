# ADR-019 — Exhaust glows in its drive's color, drawn by an additive pass of camera-facing quads

Status: **accepted** · 2026-10-01

## Context

The owner put exhaust into the MVP on 2026-10-01, colored by drive. A drive has no mesh (design §7), so until now only a ship's speed told an Ion design from a Fusion one, and countering the enemy means reading its designs (design §10). An exhaust is light, so it brightens what is behind it. The mesh pipeline is opaque, and an effect there fades by darkening (ADR-011, task 3.5). ADR-006 leaves passes to the task that needs one. Q4 counts every draw: 200 ships and 40 structures already cost a draw call each.

## Decision

1. **`Neuron::GlowPipeline` in `NeuronClient` draws glows.** A glow is a point, a radius and a linear color with its brightness included. Its quad faces the camera along the screen's right and up, which `Outpost::Camera::ScreenAxes` reads from the view matrix. The pixel shader fades from the color at the center to nothing at the rim, as (1 − r²)², and the blend adds the result to the scene. Glows are tested against depth but write none, so a hull hides the part of a glow behind it while glows overlap in any order. A frame's glows are one instanced draw. The vertex shader makes each quad's corners from `SV_VertexID`, so the only buffer is the instances, one slot of up to 4,096 per frame in flight. Glows past that are dropped. The shaders are `GlowVS.hlsl` and `GlowPS.hlsl`, shader model 5.1, like the others. The pipeline knows no game concept.
2. **Each `exhaust` hardpoint (ADR-018) makes four glows**, in `Outpost::AddExhaustGlows`:
   - a core just out of the nozzle;
   - three puffs streaming the way the hardpoint points, each further out, smaller and dimmer.

   The plume lengthens from 0.4 to 1.6 times its full spacing, and the glows brighten, as the ship's speed rises from rest to 50 m/s, an Ion Medium's cruise. The speed is how far the view moved the ship since the last frame, so it is presentation and needs nothing from the server but positions. These looks are constants in `Hardpoints.cpp`, as task 3.5's are in `CombatEffects.cpp`.
3. **An exhaust's color is its drive's, and `Models.json` holds it.** Each entry of `exhausts` gives a drive's identifier and a linear color. `constructorExhaust` is for the Constructor, which has no drive. The colors are Ion cyan, Fusion magenta and the Constructor a pale gray, final with the team colors (owner, 2026-10-01). Neither is the blue or the orange-red of a side, so drive and side read apart.
4. **`EntityView` carries a warship's drive.** The server fills it from the ship's design for every player's snapshot, as it does the hull. The client needs it for the enemy's ships, whose designs a snapshot does not list.
5. **A shot leaves from its shooter's gun.** `CombatEffects` asks `GameClient`, through a `MuzzleLocator`, for the shooter's `gun` hardpoint nearest the target, where the view draws the ship in that frame. So a beam stays on a ship that moves while it fires: an Ion Small at 78 m/s moved about 20 m during a 0.25 s beam. When the ship is gone or has no gun, the shot leaves from where the server says it was fired. A gun sets where a shot starts across the ground, not how high: effects keep task 3.5's fixed height, which keeps them above the ships.

## Consequences

- **Q4 gains one draw call and up to 1,600 instances**: 200 ships with at most two exhausts each, at four glows an exhaust. This is not measured yet. The owner's `--measure --stress` run includes it, since exhaust ships.
- **The look is unproven.** Glow sizes, brightness and the plume's growth are first guesses, and only the owner's run says whether drive and speed read from the RTS camera.
- **`GameAppTests` checks the math without a GPU:** where hardpoints land on a turned and scaled model, which gun a shot leaves from, that an exhaust streams behind the ship in its drive's hue and grows with speed, and the screen axes. `GameLogicTests` checks that every player's snapshot carries a ship's drive.

## What this forecloses

- Glows sorted or alpha-blended: light only adds, so a glow cannot darken or hide what is behind it.
- Lights and docking hardpoints until a task gives them a meaning. Their tags fail to load until then (ADR-018).
