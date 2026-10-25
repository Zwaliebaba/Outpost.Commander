# Owner decisions, 2026-10-25

These are the answers to the reference's open questions, which the owner gave in the session. Each one decides what the game *should* do. None changes the reference, which still shows the code at `a053fe2`, and none changes the game yet.

Under AGENTS.md §6 and the authority paragraph, each becomes work in the repository: an ADR edited in place, or a new ADR when the decision is new, in the same change as its code. The reference records the decisions here, so they are written down before the code is.

## Conflicts (code against a decision)

| # | Question | Decision | What it takes |
|---|---|---|---|
| C-001 | The minimap and the node figures color sides as own and enemy (`Hud.cpp:26-27`), while the world colors them by player number from `Models.json`. In a world, seat 2 sees its own ships red-orange in the world and blue on the minimap. | **The HUD follows the sides.** The minimap and the figures use each side's `Models.json` color, so a side keeps one color everywhere. | `Hud.cpp` reads the side colors the client already has (`ModelCatalog`); ADR-043 decision 7 and ADR-085 rewritten in place; contrast of the new figure colors rechecked against ADR-062. |
| C-002 | The Human Command Station mesh is about 2:1 deep. It is drawn 90 m by about 181 m on a 90 m footprint at every level. | **Rework the mesh.** The Human station's L1–L5 art goes back toward a square plan. | Owner's art in Blender; `Tools/BakeMeshes.py` bakes it again. No code change. ADR-064's "level 1 is square" holds again. |
| C-003 | The designer reserves 6 bar rows (`Hud.cpp:1087`) but draws 7 whenever a module has sight. The Build row runs about 13 units into the footer. | **Fix `ExtentOf`.** Reserve a row per bar drawn. | The window grows to 938 units: it still fits 1080p and still reaches 110 %. Add a `HudTests` case that no bar meets the footer. |
| C-004 | A lit footprint ring uses the side's ship-line color. ADR-042 decision 2 says the structure's own line color. | **The code wins.** | ADR-042 decision 2 rewritten in place. |
| C-005 | The Large hull card's numbers may be cut short at 1080p in Cascadia Mono. This is unproven. | **The owner checks it on Windows.** | Open until that run. |

## Gaps (nothing decided before)

| # | Question | Decision | What it takes |
|---|---|---|---|
| G-005 | A Shipyard and a Repair Bay need a held sector; a Research Lab and a Defence Platform need nothing, so Defence Platforms can creep into enemy sectors. | **Both need a held sector.** | The placement check in `Simulation.cpp:2113-2133`; ADR-056 gains the rule. |
| G-004 | A Mining Rig can be placed in a held sector while it is suppressed, and earns nothing until the suppression lifts. | **Allow it.** | ADR-056 states it. |
| G-014 | The art has Shipyard L4–L5 and Research Lab L5; the data stops at L3 and L4 (Phase 3 gate K3). The extra levels are loaded and never drawn. | **Keep them for later.** | ADR-064 notes that the art waits for content. |
| G-015 | The Mining Rig mesh has a `gun` hardpoint and no weapon. The Defence Platform has 6 gun hardpoints for 1 gun. | **Remove the rig's gun hardpoint.** The platform's six are its muzzles: a shot leaves from the one nearest its target (`Hardpoints.cpp:206-220`). | Owner's art, baked again; ADR-018 documents the muzzles. |
| G-023 | The Controls window says "Select a ship, or a structure of yours", but a click picks only your own ships. | **Own ships only; fix the text.** | `KeyBindings.h:94` reworded, for example "Select a ship or a structure of yours". |
| G-025 | A command the server refuses is never told to the player (`InProcessServer.cpp:482`). | **Tell the player.** The server answers a refusal with its reason, and the HUD shows it. | A protocol change, so a new ADR (and ADR-060 / ADR-080 rewritten in place); an alert or hint text for each refusal. |
| G-028 | A seat nobody has taken is its deputy's from tick 0, so the first join of a new world opens an away report of everything since the world began. | **No report on the first join.** | The away report starts only once a player has held the seat (`AwayReports.cpp`, `SeatController.cpp`); ADR-079. |
| G-034 | The window stays hidden while loading (ADR-049), and Join world freezes the menu for up to 10 s with no sign. | **A connecting sign only.** Keep the hidden startup; make the join asynchronous, with a "Connecting to {server}…" line on the menu. | `WinMain.cpp:349-358`, `QuicTransport`; ADR-078 decision 9 and ADR-086. |
| G-024 | No cap on scheduled orders, and no cancel in the Orders window. *Corrected in the session:* a ship waits on at most one scheduled order, and any direct order to it takes it out (`Simulation.cpp:3240-3253, 3305-3320`). So the orders are already bounded by the ships, and Stop cancels one. | **A cancel control in the Orders window, and no new cap.** The window also says that a direct order leaves a scheduled one. | `Hud.cpp` Orders window and a command to cancel; ADR-080 states the rule as it is. |
| G-036 / G-039 | Only the HUD's own controls light under the pointer. The Orders window's Give button never lights, and nothing has a pressed state. | **Light Give only.** Windows otherwise stay as ADR-088 says, with no pressed state. | `Hud.cpp:1979` passes the hovered flag; ADR-088's Consequences name Give. |

## The reference's own open items

| Item | Decision |
|---|---|
| The Hulls table overflows its artboard (V item 10, failed twice) | **Fix it anyway**, by a narrow fix: the table's headers may break at their camelCase joins. |
| The minimaps' sample marks leave out the player's own side (V2 defect C) | **Fix it.** |
