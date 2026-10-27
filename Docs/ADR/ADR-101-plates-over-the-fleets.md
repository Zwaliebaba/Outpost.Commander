# ADR-101 — Plates over the fleets on the screen

Status: **accepted** · 2026-10-27
Owns: screen-fleets, fleet-plates, plates-from-view-width

## Context

Far out, a battle is dozens of ships, and a bar over each hurt one ([ADR-088](ADR-088-selection-and-commands.md) decision 5) reads as noise. The war game the owner pointed to counts a squad, not a soldier: a plate over each squad with its count and a badge in its side's color, which reads at a glance and stands in for every health bar. The game has no squads. A fleet is ships near each other, and the enemy's groups are not in the snapshot, so a fleet has to be found from what the screen shows (owner, 2026-10-25).

Two things make that finding hard. Plates of one side that meet have to become one, so fleets depend on the zoom: what is two fleets close in is one far out. And a plate that jumps each time a ship dies or joins reads as a flicker, so a fleet has to keep its identity from one frame to the next.

## Decision

1. **A fleet is a side's ships near each other on the screen** (`ScreenFleets`), found afresh each frame from where the camera shows them, and nothing in the simulation.
   - **Two ships of one owner are of one fleet when their plates would meet on the screen,** a plate's width apart across and its height apart up and down, or when they stand as near on the ground as a formation keeps its ships: within `ScreenFleets::FORMATION_REACH_RADII`, 3.5, footprint radii of the widest of their side's ships on the screen. A formation keeps three radii of its widest ship between neighbors ([ADR-010](ADR-010-movement-and-pathing.md) decision 4), so at the default view of 500 m a formation of Medium hulls stands about 160 pixels apart on a 1920-pixel-wide screen, computed from the hull's 14 m radius, and the screen's test alone would give each ship a plate of its own. The widest is the side's, since a formation's spacing is its widest ship's, which a small ship beside it does not know.
   - **Two fleets of one owner whose plates would meet become one,** however far apart their ships are, until no two meet.
   - **A fleet keeps its identifier while it shares a ship with the last frame's,** the largest shares choosing first and a tie going to the older identifier, so the same frames name the same fleets. A fleet that parts keeps it in its larger part.
   - **A plate moves with its ships,** by how its ships still in the fleet moved on the screen, so a pan or a turn of the camera carries it. When a ship joins, leaves or is lost, it starts where it stood and settles where it belongs over `ScreenFleets::SETTLE_SECONDS`, 0.15 s, rather than jumping there.
2. **A plate is a count over a disc in its side's color, ringed in white** (`Hud::PlateOver`, `Hud::Plate`), with no panel behind it, `Hud::PLATE_WIDTH_UNITS` wide and `Hud::PLATE_HEIGHT_UNITS` tall.
   - **The disc** is its side's color as `Models.json` gives it, and the pirates' violet for theirs ([ADR-085](ADR-085-one-meaning-per-color.md)).
   - **The insignia on the disc** is white and names the fleet's commonest kind of ship: one, two or three chevrons for a warship's hull by its size among the match's hulls, smallest first, and a plus for a Constructor (owner, 2026-10-27). A tie goes to the larger hull, and a warship before a Constructor.
   - **The ring is an arc of the fleet's hit points against their full,** white from the top clockwise, since a ship is not one hit as a soldier is, and dark for the rest, in the halo's color. White is none of health's colors. A whole fleet's ring is one sprite; a hurt fleet's is drawn in 16 pieces round the circle.
   - **The count stands over the disc,** in Bahnschrift's bold semicondensed face (`Hud::Typeface::Plate`), white on a dark halo ([ADR-030](ADR-030-typography-and-sprites.md) decision 8). A plate of one ship has its insignia and no count.
   - **A plate stands centered over its fleet's ships, level with the highest of them on the screen,** a bar's lift above it, as a bar stands over its entity ([ADR-088](ADR-088-selection-and-commands.md) decision 5).
   - **Plates of two sides that meet move apart,** the higher up and the lower down, until they do not. Plates of one side never meet, since decision 1 joins them.
   - **The plates are drawn after the bars and before every layer,** so under every panel and window. A plate is laid out at the interface's scale, the player's factor included, not the screen's alone as the bars are ([ADR-070](ADR-070-interface-scale.md) decision 7), since its count is text from the atlas, which is drawn at that scale.
   - **A click on one of the player's own plates selects its fleet,** and Shift adds the fleet to the selection (`PlayerControls::SelectFleet`); whatever was armed for the next click is dropped, as a click on a ship's bar drops it ([ADR-066](ADR-066-production-status-on-the-hud.md) decision 1). A plate under a panel or a window takes no click there, and another side's plate takes none, so a click on it is the world's.
3. **The plates stand over the fleets while the camera's view is wider than 300 m** (`Hud::PLATES_FROM_VIEW_WIDTH_METERS`; owner, 2026-10-27), so the default view of 500 m shows them. There a fleet's plate stands for its ships' bars: a ship shows its own bar only while the pointer is over it, and every ship's and structure's shows while Alt is held, with no plate. A structure keeps its bar. Closer in there is no plate, and a hurt ship shows its bar as a structure does (ADR-088 decision 5).

## Consequences

- **What a plate costs of `UiPipeline::MAX_QUADS`, computed, not measured:** a whole fleet's plate is its ring, disc and insignia, three quads, and two for each digit of its count, the halo's and the glyph's; a hurt fleet's has up to 17 pieces of ring in place of the ring's sprite. A battle of 300 hurt ships each a fleet of its own on the screen is 300 plates of 19 quads, 5,700 of the 8,192.
- **A ring of 16 pieces strays from the circle by a quarter of a pixel** at its radius of 13 units at 1080p, computed as the radius times one less the cosine of half a piece's angle. A hurt fleet's ring is not smoothed at its edges as the whole ring's sprite is.
- **Finding the fleets compares every two ships on the screen,** 44,850 pairs for 300 ships, each frame while plates show. It is not measured.
- **The halo is drawn into the atlas** for the plate's face alone (ADR-030 decision 8).
- **Tests.** `ScreenFleetsTests` checks two formations 40 m apart as two fleets at the default view and one once the camera pulls back, two small ships at a large one's spacing as its fleet and as fleets of their own without it, another side's widest ship setting no reach of this side's, a plate centered over its ships and level with the highest, a fleet that loses a ship keeping its identifier with its plate where it stood and then settled, a fleet that parts keeping it in its larger part, two sides never one fleet, and two fleets whose plates would meet becoming one. `HudTests.DescribesAFleetsPlate` checks the count, the insignia of the commonest kind with its ties, the share of hit points left, the side's color and the pirates', and whose a plate is. `HudTests.LaysOutAPlateOverAFleet` checks the disc centered a lift above the foot in its side's color, the insignia on it in white, a whole fleet's ring as a white sprite and a hurt one's white from the top clockwise for its share, the count over the disc with its halo at 4.5:1 over white, no count for one ship, and a click on the player's own plate alone. `HudTests.MovesPlatesOfTwoSidesApart` checks two sides' plates parted by a plate's height, one side's left where they stand, and a plate under a panel taking no click. `PutsABarOnlyOverWhatShowsOne` checks decision 3's bars. `GameClient` needs D3D12 and Windows, so CI builds it, and the owner's run sees it.

## What this forecloses

- **A fleet the simulation knows,** for orders given to it in advance: that is the horizon's.
- **A plate over structures.** A structure keeps its bar, near or far.
- **A plate that stands for an enemy's group the player does not see.** A plate is made only of the ships the snapshot shows.
