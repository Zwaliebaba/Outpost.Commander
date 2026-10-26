# ADR-019 — Exhaust glows in its drive's color, drawn by an additive pass of camera-facing quads

Status: **accepted** · 2026-10-03
Owns: glow-pipeline, exhaust-glows, exhaust-colors, shot-muzzles

## Context

A drive has no mesh (Game §ships.drives), and countering the enemy means reading its designs, so a drive has to show some other way. Exhaust shows it, colored by drive (owner, 2026-10-01). An exhaust is light, so it brightens what is behind it. The mesh pipeline is opaque, and an effect there fades by darkening (ADR-011). ADR-006 leaves passes open.

## Decision

1. **`Neuron::GlowPipeline` in `NeuronClient` draws glows.** A glow is a point, a radius and a linear color with its brightness included. Its quad faces the camera along the screen's right and up, which `Outpost::Camera::ScreenAxes` reads from the view matrix. The pixel shader fades from the color at the center to nothing at the rim, as (1 − r²)², and the blend adds the result to the scene. Glows are tested against depth but write none, so a hull hides the part of a glow behind it while glows overlap in any order. A frame's glows are one instanced draw. The vertex shader makes each quad's corners from `SV_VertexID`, so the only buffer is the instances, one slot of up to 4,096 per frame in flight. Glows past that are dropped. The shaders are `GlowVS.hlsl` and `GlowPS.hlsl`, shader model 5.1, like the others. The pipeline knows no game concept. It also takes an optional sprite, and with one a glow is the sprite's color times its own in place of the soft spot ([ADR-026](ADR-026-explosions-and-particles.md) decision 1); the phosphor glow over the whole scene is [ADR-091](ADR-091-a-phosphor-glow.md)'s, a different pass.
2. **Each `exhaust` hardpoint (ADR-018) makes four glows**, in `Outpost::AddExhaustGlows`:
   - a core just out of the nozzle;
   - three puffs streaming the way the hardpoint points, each further out, smaller and dimmer.

   The plume lengthens from 0.4 to 1.6 times its full spacing, and the glows brighten, as the ship's speed rises from rest to `EXHAUST_FULL_SPEED_METERS_PER_SECOND`, 50 m/s, about an Ion Medium's cruise, in `GameClient.cpp`. The speed is how far the view moved the ship since the last frame, so it is presentation and needs nothing from the server but positions. The plume's looks are constants in `Hardpoints.cpp`.
3. **An exhaust's color is its drive's, and `Models.json` holds it.** Each entry of `exhausts` gives a drive's identifier and a linear color; `constructorExhaust` is for the Constructor, which has no drive. The Ion Drive's is cyan, the Fusion Drive's magenta, the Pulse Drive's a lime green, and the Constructor's a pale gray (owner, 2026-10-01; the Pulse Drive's, owner, 2026-10-03). None is the blue, the orange-red or the violet of a side, so drive and side read apart.
4. **`EntityView` carries a warship's drive.** The server fills it from the ship's design for every player's snapshot, as it does the hull. The client needs it for the enemy's ships, whose designs a snapshot does not list.
5. **A shot leaves from its shooter's gun.** `CombatEffects` asks `GameClient`, through a `MuzzleLocator`, where the shot's gun is: the shooter's `gun` hardpoints, nearest the target first, picked by the shot's gun index (`ShotView::gun`), so that a structure's further guns ([ADR-064](ADR-064-structure-upgrades.md) decision 11) fire from further hardpoints, where the view draws the ship in that frame. So a beam stays on a ship that moves while it fires. When the ship is gone or has no gun, the shot leaves from where the server says it was fired. A gun sets where a shot starts across the ground, not how high: every effect stands at one height above the ground (`EFFECT_HEIGHT_METERS` in `CombatEffects.cpp`), which keeps it above the ships.

## Consequences

- **`GameAppTests` checks the math without a GPU:** where hardpoints land on a turned and scaled model, which gun a shot leaves from, that an exhaust streams behind the ship in its drive's hue and grows with speed, and the screen axes. `GameLogicTests` checks that every player's snapshot carries a ship's drive.

## What this forecloses

- Glows sorted or alpha-blended: light only adds, so a glow cannot darken or hide what is behind it.
- Docking hardpoints, and lights other than a beacon ([ADR-018](ADR-018-nmf-and-hardpoints.md) decision 9), until a decision gives them a meaning: their tags fail to load.
