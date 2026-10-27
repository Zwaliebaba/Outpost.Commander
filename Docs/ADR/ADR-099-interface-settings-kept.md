# ADR-099 — The interface's scale and the places of the windows the player moves are kept from one session to the next

Status: **accepted** · 2026-10-27
Owns: interface-settings-file, kept-window-places, kept-interface-scale

## Context

A window keeps its place, and the interface its scale, while the game runs ([ADR-031](ADR-031-floating-windows.md) decision 1; [ADR-070](ADR-070-interface-scale.md) decision 5). A player who arranges the windows and sets the scale did so again every session. Both are now kept across sessions (owner, 2026-10-27).

The same player may start the game on another screen, or at another scale, than the one it left it on. A place kept as a distance from the screen's top-left corner moves a window left against the right edge into the middle of a wider screen, and off the edge of a narrower one, where the clamp of ADR-031 decision 2 leaves only 120 units of it on the screen.

The game is packaged as MSIX ([ADR-001](ADR-001-win32-shell.md)). Windows keeps what a packaged program writes to the local application data in the package's own data, and removes it with the package.

## Decision

1. **What is kept: the scale's step the player last took, and the place of each window the player has moved.**
   - The step is kept as its percentage. A step down that a screen or a match makes ([ADR-070](ADR-070-interface-scale.md) decision 4) is not kept, nor a step up the screen has no room for, so that the next session starts at the player's own and comes down only as far as its screen needs.
   - A window not yet moved is not kept, and opens where the HUD first puts it (ADR-031 decision 2).
   - A place is kept from the corner of the screen nearest the window's middle: the distance from that corner's side edge to the window's left edge, and from its top or bottom edge to the window's top, in reference units (`InterfaceSettings::Place`, `PlaceOf`, `CornerOf`). A window left against the right edge stays against it on a wider screen, a narrower one and at another scale, and one left low stays as far above the foot.
2. **Where: `Outpost Commander\Interface.json` in the player's local application data** (`FOLDERID_LocalAppData`), which Windows keeps in the package's own data. The shell finds the folder (`LocalInterfaceFile`) and hands the path to `GameClient::KeepInterfaceIn`. `GameApp` reads and writes the file (`LoadInterfaceSettings`, `SaveInterfaceSettings`) with NeuronCore's JSON (`ParseJson`, `QuoteJson`).
3. **The file is JSON of version 1:** its `version`, its `scalePercent`, and under `windows` a member for each kept window, by name, with the corner it is kept `from` and its `xUnits` and `yUnits`:

   ```json
   {
     "version": 1,
     "scalePercent": 125,
     "windows": {
       "designer": {"from": "topRight", "xUnits": 834, "yUnits": 16}
     }
   }
   ```

   - The windows' names are `designer`, `research`, `production`, `controls`, `orders`, `away` and `leave`, and a corner's are `topLeft`, `topRight`, `bottomLeft` and `bottomRight`. A name is never changed, since a file an earlier build wrote names its windows so.
   - A file of another version, or with none, is not read (`ReadInterfaceSettings`).
   - A member that is not as the game writes it is left out, and the rest is read: a scale that is no step, a corner with no name, a distance that is no number or more than 100,000 units from its corner. So is a window this build does not have, so that a file a later build wrote still gives what this one knows.
4. **When:** the file is read once, as the shell starts the client and before its window shows. It is written when the player lets go of a window, when a step the player takes changes the scale, and as the game closes, and only when what it would hold has changed since it was last written or read. It is written to `Interface.json.new` beside it, which then replaces it, so that a crash never leaves half a file.
5. **A file that cannot be used is passed over.** One that is not there, cannot be read, is not JSON or holds no settings of this version leaves the game to start as it would without it, and in Debug the game says so to the debugger. Neither reading nor writing it ever stops the game. A write that fails is tried again at the next change, or as the game closes.
6. **A kept window stands at its place every frame of a match.** `GameClient` works its corner out from the screen's size and the scale that frame (`PlaceKeptWindows`) and stands it there (`WindowManager::Place`), except while a window is dragged; the HUD then keeps it on the screen as it keeps any window (ADR-031 decision 2). Letting go of a window keeps its place as the last frame laid it out (`KeepDraggedWindow`).
7. **A measurement run neither reads nor writes the file:** `--measure`, `--load` and `--stress` measure every frame with the interface as it first is, at 100% and with no window moved.

## Consequences

- **The interface comes back as the player left it,** on the same screen; on another, each kept window keeps to its corner and the scale comes down as far as the screen needs.
- **Within a session too, a moved window keeps to its corner** across a step of the scale or a new size of screen, where it kept its distance from the top-left corner and was clamped.
- **A window that grows keeps its top's distance from its corner.** The production window grows at its foot (ADR-043 decision 6), so one kept from the bottom grows past it, as it did before.
- **The file goes with the package.** Uninstalling the game removes it; nothing else needs to.
- **The tests.**
  - `InterfaceSettingsTests.KeepsTheScaleAndThePlacesThroughTheFile` writes and reads the settings back through the text and through a file in a folder it makes, and leaves nothing beside the file.
  - `StartsWithoutAFileItCannotRead` reads no file, an empty, a cut-off and a foreign one, one with no version and one of another.
  - `LeavesOutWhatItDoesNotKnow` reads a file of a later build: a window this one does not have, a scale that is no step, a corner with no name and distances that are text or past any screen, and keeps the one window that is as written.
  - `KeepsAWindowFromTheCornerNearestIt` keeps the designer from the top-right corner and production from the bottom-left, and stands them on a wider screen and at 125%.
  - `ClampsAKeptPlaceOnASmallerScreen` brings a place off the screen back to its title bar and 120 units of its width.
  - `KeepsTheScaleAsItsStep` holds the percentages of every step.
  - `WindowManagerTests.PutsBackAPlaceKeptFromAnEarlierSession` stands a window at its kept place, where it takes no slot.
  - `GameClient` and the shell need Windows and D3D12, so they have no test; CI builds them.

## What this forecloses

- **Reopening the windows that were open.** Every window starts closed, as it does in a new match.
- **A place for each screen.** A window has one kept place, measured from a corner, whatever the screen.
- **Roaming the settings to another machine.** They are in the local application data, not the roaming.
