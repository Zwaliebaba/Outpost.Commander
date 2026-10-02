# ADR-028 — The grid is pixel-wide lines, the brightest stars are crosses, and the scene's colors put the fleets first

Status: **accepted** · 2026-10-02 · Supersedes [ADR-022](ADR-022-natural-starfield.md) decisions 3 and 8

## Context

Once the asteroids showed their ridges as one-pixel lines (ADR-027), two parts of the scene no longer matched them. The grid was drawn as flat strips 1.5 m and 4 m wide on the ground (ADR-022 decision 8). A strip's width is in meters, so zoomed in its lines were heavy bars, the major ones most, and zoomed out they thinned below a pixel and broke up. The brightest 70 stars were soft starburst sprites 9 to 36 pixels in radius (ADR-022 decision 3), the one photographic element left in a scene of vector lines. On 2026-10-02 the owner chose a grid of uniform one-pixel lines every 100 m in a dim blue-gray, with no major lines, and fewer, smaller stars drawn as four-point crosses.

The owner's run of a full battle the same day, with every model lined (ADR-027), showed three faults in what the eye goes to first. The rocks' lines, at twice a light gray, were the brightest lines on the screen, ahead of both fleets, though the rocks are only terrain. The fireball and debris kept DeepSpaceOutpost's red, so a Human ship blowing up in its own fleet read as Tarkan there. And every beam was the same pale blue, the Humans' own family, so Tarkan fire into the Human fleet read as Human fire. The owner asked for all three to be fixed. The next run put the fleets first, but showed the amber blasts as flat beige patches, dust rather than fire, the beams faint at one or two pixels, and the health bars as the most saturated marks on the screen, a little off their ships and green over the Tarkan alike. The owner asked for those to be fixed too.

## Decision

1. **The grid is one line list drawn with `MeshPipeline::DrawLines`.** It has a line every 100 m along x and along z across the 2,000 m map, 21 each way, all alike. Each is a pixel wide at any zoom, the weight of the ridges. Its color is (0.012, 0.013, 0.019) in linear color, a dim blue-gray. Lit from above, it shows at about (26, 27, 34) in sRGB, between the old minor and major lines' brightness, and well below the ridges, which are twice the rock's color. Lines write no depth, so the grid no longer cuts through the lower half of whatever is drawn over it.
2. **`Neuron::StarPipeline` takes a `Shape`: `Spot`, `Sprite` or `Cross`.** `Spot` is ADR-021's Gaussian and `Sprite` ADR-022's sprite, which needs its texture. The new `Cross` draws, in `StarCrossPS.hlsl`, two lines along the screen's axes and a dot of one pixel's radius at the center. Each line is one pixel of the screen wide, filtered over a pixel so that it stays smooth as the camera turns the sky, and fades linearly from the star's color at the center to nothing at the quad's edge. The shader finds a pixel's size in the quad from the corner's screen derivatives, so it needs no new constant.
3. **The brightest 25 stars are drawn as crosses, 6 to 16 pixels in radius on the reference frame.** That is a handful on the screen at a time. Their colors and brightness are as before, the star's tint at 0.6.
4. **A rock's lines are 1.35 times its color, not 2.** Its faces stay at 0.65, so its lines still stand out from them, but they no longer outshine the ships and structures, whose lines stay at 2 (ADR-027).
5. **A blast runs from white-yellow to amber, not red, and its fireball is born hot.** The fireball is between (0.42, 0.37, 0.23) and (0.4, 0.24, 0.08) in linear color, a fifth brighter than DeepSpaceOutpost's, and each puff is born at (0.55, 0.52, 0.44), near white and half as bright again, and cools to its own color over its first 0.4 s. The debris is between (0.22, 0.13, 0.04) and (0.337, 0.22, 0.08), and the smoke stays gray (ADR-026). No blast is in either side's color.
6. **A beam is its shooter's side's color, a third of the way to white, and 4 m wide rather than 3.** `CombatEffects::At` takes a `BeamTint` from `GameClient`, which finds the shooter in the view and lightens its set's color. A shooter the view does not hold keeps the old pale blue. Tracers, flashes and sparks keep their yellow, which belongs to neither side.
7. **A health bar's full-length back is its side's color at 0.3**, dark blue or dark red-brown, rather than dark gray, so that a bar says whose it is; its fill keeps green, amber and red for how hurt. It sits 1.5 m off its footprint rather than 4, so that it reads as its ship's.
8. **The minimap stays where it is.** It covers the screen's bottom-left corner, but the camera can center any point of the map, so nothing stays under it.

## Consequences

- **`starburst.dds` is no longer drawn.** It stays in the package and in the repository, where `TextureDataTests` reads it, and the `Sprite` shape stays in `StarPipeline`, so the old look is a one-line change back.
- **The grid costs the same one draw**, now of 84 vertices instead of strips.
- **Nothing marks 500 m any more.** If the owner misses that scale, the major lines come back slightly brighter, never wider.
- **A cross's lines are a pixel of the screen**, so at 4K they are half as wide, relative to the scene, as at the reference 1080p, as the ridges are.
- **The look is unproven.** It was written without a GPU; only the owner's run shows it.
- **Two sides with the same color would share a beam color.** The sets give every player a color of its own today.
