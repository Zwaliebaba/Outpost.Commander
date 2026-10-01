# ADR-021 — The sky is stars at infinity, drawn as points, with a Milky Way band made of nothing but stars

Status: **superseded** by [ADR-022](ADR-022-natural-starfield.md) · 2026-10-01

## Context

Until now the battlefield was drawn over a dark blue clear color, with a grid on the ground (ADR-011). On 2026-10-01 the owner asked for that color to go and for a space backdrop in its place: stars and a galaxy, with the galaxy made of real stars rather than a smear. The owner then made three choices: keep the grid but dim it, make the galaxy a Milky Way band across the sky rather than a separate spiral galaxy, and draw it from points alone, with no diffuse glow under the core.

Three facts about the view set the problem. The RTS camera looks down at 40 to 70 degrees with a 45-degree vertical field of view (ADR-012), so it only ever sees the lower half of the sky and never the horizon. Q and E turn it freely. Panning moves it but does not change which way it looks. ADR-006 leaves passes to the task that needs one, and this task needs one.

## Decision

1. **The sky is a fixed set of stars at infinity, and `Neuron::StarPipeline` in `NeuronClient` draws them.** A star is a direction, a spread and a linear color with its brightness included. The vertex shader builds each star's quad from `SV_VertexID` as a strip of four vertices. It transforms the direction with w = 0, so the camera's position drops out and only turning the camera moves the sky. It then offsets the corners by a number of pixels, scaled by w so that the size survives the divide, and puts z at w/2, so that only a star behind the camera is clipped. The pixel shader draws a Gaussian with its value at the rim subtracted, so a quad's edge never shows, and the blend adds the result to the frame. The sky is drawn first, straight after the clear, with no depth test and no depth write, so everything drawn after it covers it. The stars are uploaded once, by `Renderer::CreateStaticBuffer`, and drawn in one instanced draw. The shaders are `StarVS.hlsl` and `StarPS.hlsl`, shader model 5.1, like the others. The pipeline knows no game concept.
2. **Stars are points, not a texture.** In a cube map or panorama each star is a texel, which blurs when the view magnifies it and shimmers when the view shrinks it. That blur is the smear the owner ruled out. A cube map sharp enough for 4K would also need six faces of about 2,048 texels square, or 100 MB at four bytes a texel. A point stays sharp at any resolution. Its spread is in pixels on ADR-006's 1920×1080 reference frame, scaled by the same uniform factor as the HUD, so the sky looks the same on every monitor. Each star is at least 0.7 pixels across at that scale, which keeps a slowly turning sky from flickering.
3. **`Outpost::BuildStarfield` in `GameApp` makes the sky from a fixed seed, so it is the same every run.** It uses its own SplitMix64 rather than `<random>`, whose distributions differ between standard libraries. It has three populations:
   - 14,000 field stars spread evenly over the whole sky;
   - 260,000 candidates for the band's disk, crowded toward the center along the band and thicker there;
   - 80,000 candidates for the bulge round the center.

   Clouds and dust are where candidates are dropped. Smooth noise over the sky decides where a candidate is kept: it is kept with the cloud noise's chance, between 0.15 and 1. The dust then removes up to 97% of what remains in a rift along the plane toward the center, and 78% in filaments across the band. So every feature of the galaxy is made of stars, and the only thing drawn is stars. Of the 354,000 stars made, 193,390 are kept.

   A star's light follows a power law. The count of field stars brighter than a given light falls as that light to the power −3/2, as it does for stars spread evenly through space. For the band and bulge it falls as the power −2, so few of their stars are bright. A brighter star is drawn wider, up to a spread of 1.6 pixels, rather than brighter than a peak of 0.75. Colors come from seven blackbody tints between 3,000 K and 10,000 K, each mixed with white so that 65% of the tint remains. The bulge has more of the red tints. These looks are constants in `Starfield.cpp`, like task 3.5's in `CombatEffects.cpp`.
4. **The galaxy's center is placed for the default view.** Its direction is (0.28, −0.79, 0.55), normalized. That is 52 degrees below the horizon and to the right of the camera's starting yaw, which looks along +z. The band's plane runs through (−0.80, −0.30, 0.50), so the band crosses the screen diagonally from the center up to the left. At the starting yaw, the center is on screen at every zoom on 16:9 and 16:10 screens.
5. **The clear color is black, and the grid is half as bright.** Its minor lines are now (0.025, 0.045, 0.07) and its major lines (0.05, 0.09, 0.14), in linear color. The grid stays because the sky does not move when the view pans. The grid shows the ground moving, and it is all that shows where the map ends.

## Consequences

- **The band is out of view for part of a turn.** A great circle cannot stay in the lower half of the sky at every yaw. In 58–60% of the yaws, more than 5,000 band stars are on screen. In the rest, only field stars show. This was measured by projecting the generator's output through the camera's view and projection at the starting position, in 5-degree steps of yaw. The view widths were 150 m, 500 m and 1,600 m, on a 16:9 screen, and the count was the band and bulge stars inside the screen.
- **Q4 gains one draw call, 773,560 vertex shader invocations, and the stars' pixels.** Each of the 193,390 stars is four vertices. Most of them are clipped, since 83,000–100,000 stars are on screen at the starting yaw. Most quads are about 4 pixels on a side on the reference frame, so the pixel cost is a few million additive pixels at 1920×1080. On the 2880×1920 panel, where the scale is 1.5, it is about 2.25 times that. None of this is measured on a GPU yet. The owner's `--measure --stress` run includes it.
- **Building the sky takes a moment at start.** It took 0.27 s optimized (`-O2`) and 0.67 s unoptimized (`-O0`) with g++ 13 in the Linux container. MSVC Debug was not measured and is likely slower. The stars take 5.4 MB of video memory, at 28 bytes each.
- **Whatever is opaque hides the sky.** Grid lines cut through the band, and hulls and asteroids cover the stars behind them.
- **The look is checked offline only.** The previews were made by compiling the generator with g++ in the Linux container, then projecting and splatting its stars in Python, emulating `StarVS.hlsl` and `StarPS.hlsl` and the sRGB encoding, with the dimmed grid drawn over them. The game itself has not run it. Only the owner's run on Windows says whether it reads as intended and stays behind the play.
- **`GameAppTests` checks the generator without a GPU.** It checks that:
  - the sky is the same every time;
  - the star count stays between 100,000 and 400,000;
  - every star is a unit direction, inside its size and brightness limits;
  - the galaxy's center and pole are at right angles;
  - more than 80% of the stars are within 10 degrees of the plane;
  - more than five times as many stars lie within 15 degrees of the center as of the point opposite it;
  - the center is on screen at the starting yaw, at every zoom, on 16:9 and 16:10 screens.

## What this forecloses

- A textured sky, such as a cube map or a panorama, and diffuse light such as nebulae or a glow under the bulge, without changing this ADR.
- Parallax: the sky never moves when the view pans.
- A different sky per match or map. There is one seed.
- Stars that twinkle or move.
