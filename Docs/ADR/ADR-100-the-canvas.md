# ADR-100 — The canvas: the interface's display list, layout, interaction and widgets are the engine's

Status: **accepted** · 2026-10-27
Owns: display-list, canvas-layout, canvas-interaction, canvas-window-stack, canvas-theme, canvas-widgets, windows-on-the-canvas, windows-table

## Context

Every window of the interface is placed by hand: `Hud.cpp` sets each row a number of reference units from its window's corner, which `Painter` scales, and works each window's height out a second time apart from its layout. The pointer's hover reaches a window as a parameter, and the one field that takes keys reads its own. The owner asked for generic windows, buttons and drop-downs, so that a window is built and not drawn (owner, 2026-10-27), and decided that what is generic is the engine's, in `NeuronClient`, from the first task (owner, 2026-10-27). None of it needs a game concept, so R9 puts it there, and `NeuronClientTests` tests it without a GPU. R14 rules out a toolkit such as Dear ImGui, RmlUi or Noesis, and none draws the mockup's look without being rebuilt to it.

## Decision

1. **The display list is the engine's** (`Neuron::DrawList`). It holds panels (`CanvasRect`), solid, hatched or filled with an image (`CanvasFill`); lines at any angle (`CanvasLine`); sprites (`CanvasSprite`) and texts (`CanvasText`), which name a sprite and a font by their index in the atlas; and regions (`CanvasRegion`), each with the widget it is, the caller's payload, whether it answers the pointer only, and whether it scrolls. It is divided into layers: the first, and one for each `BeginLayer`, which takes the rectangle a press on it falls in and whether it is a drop-down's overlay.
   - **Finding what is under a point:** `LayerAt` gives the front layer that holds a point. `HitAt` gives the region of that layer that takes a press, `PointedAt` the one the pointer is over, and `ScrollAt` the innermost one that scrolls. `Covers` says whether the point is on any panel ([ADR-015](ADR-015-ui-drawing.md) decision 5). A region of a layer behind takes nothing.
   - **Drawing it:** `UiPipeline::Draw` draws it into the frame's one draw call ([ADR-015](ADR-015-ui-drawing.md) decision 2), layer by layer, each layer's panels, then its lines, then its sprites, then its texts ([ADR-031](ADR-031-floating-windows.md) decision 4). A `FillStyle` says how wide a hatched panel's stripes are and which part of the image an image panel shows.
   - **Clipping a line:** `ClipToRect` cuts a line where it leaves a rectangle, as the minimap's view is cut.
   - **What the game keeps:** `Hud::Layout` is a display list and what the game keeps beside it. That is the actions and previews that its regions' payloads index, the minimap's square and how it maps the map, its windows, and the bars over the entities. `Hud::FontOf` and `Hud::SpriteOf` give a typeface's and a sprite's index. `GameClient` fills the image from the fog's texture ([ADR-052](ADR-052-fog-texture.md)).
2. **A layout measures, then places** (`Neuron::LayoutTree`), recorded afresh each frame.
   - **Nodes:** a box, which is a spacer when it grows; a column, a row, a grid of a number of columns, or a stack, each over the others; and a text. The root is the screen.
   - **Style:** each node has a size given, or a size measured from what it holds and its padding, held between its least and its most. It also has a gap, an alignment across and down, a share of the room left along its parent's axis (`grow`), and an anchor to a corner, an edge's middle or the middle, inset by a margin. An anchored node takes no room in its parent. One larger than its parent's content box less its margin, across or down, stands at that box's start that way, so that its top-left corner, where a window's title bar is, stays in the box. A node may belong to a height group, whose nodes are all as tall as the tallest of them wherever they stand, and what holds each grows with it; one measured and not drawn takes part too.
   - **Measuring and placing:** `Arrange` measures widths from the leaves up and gives them out from the root down. It then measures heights from the leaves up and places from the root down, so that a text wraps to the width it is given and its parent is as tall as its lines.
   - **Sharing room:** room left or lacked along a column or a row is shared in proportion among what grows. A node held at its least or its most keeps that, and what it could not take is shared again.
   - **Text:** a text is set as one line, as one line cut short with three dots, or wrapped ([ADR-061](ADR-061-measured-text.md) decision 3). One cut short within a most is as wide as it is then set, so that what follows it stands after its dots. A text of nothing, or of which nothing fits, is still a line tall, so that what follows it keeps its place.
   - **The measure:** `Neuron::TextMetrics` measures, cuts and wraps a line as `UiPipeline::DrawText` sets it ([ADR-061](ADR-061-measured-text.md) decision 1), naming a font by its index. `Hud::TextMetrics` is the same measure named by the game's typefaces.
   - **Units:** sizes are reference units, and become pixels as they are drawn, times the scale. A tree measured at another scale, with fonts rasterized at it, stands where it stood in units ([ADR-070](ADR-070-interface-scale.md)).
3. **What every widget shares is one interaction state** (`Neuron::Interaction`).
   - **Identifiers:** a widget's identifier is a 64-bit FNV-1a hash of its label or its index under the identifier pushed last, such as its window's. A byte before them tells a label from an index. Each is claimed once a frame (`Claim`), and the canvas fails on a second claim ([ADR-093](ADR-093-a-failed-assertion-ends-the-process.md)).
   - **Hot, active and focused:** it keeps the widget under the pointer (hot), the one pressed (active) and the field that has the keyboard (focused). Input is resolved against the display list drawn last:
     - A move makes the widget under the pointer hot, but while one is active, only that one can be.
     - A press on a panel is the canvas's, and makes the widget under it active ([ADR-015](ADR-015-ui-drawing.md) decision 5).
     - A release is never the canvas's, so it reaches the controls too. It clicks the active widget if it is let go over it.
     - The wheel is the canvas's only over a region that scrolls, whose offset takes it.
   - **Typing:** a field that has the keyboard takes every character and its keys in the order they came, Backspace, Enter and Esc ([ADR-017](ADR-017-research-and-the-designer.md) decision 8). Enter and Esc give the keyboard back, and so does a press anywhere but the field. While a field has it, `TakesKeys` says that no order, group or camera key reads one.
   - **Esc:** Esc reaches the field that has the keyboard first, then an open drop-down, and only then goes on to the windows and the controls ([ADR-031](ADR-031-floating-windows.md) decision 5).
   - **A drop-down:** one is open at most. A press outside its overlay closes it and goes no further.
   - **The store:** a scroll's offset and a caret are kept per identifier.
4. **The window stack is the engine's** (`Neuron::WindowStack`), over windows named by any value below a count, such as `WindowKind`. It keeps which windows are open and in what order front to back, where each was left and which is being dragged. A window opens at the front, is taken by its title bar, follows the pointer where it was taken hold of, and is settled where the layout could lay it out.
   - **Kept on the screen:** `Neuron::KeepOnScreen` keeps a window's title bar on the screen, and as much of its width as the caller keeps, or all of it when it is narrower.
   - **The game's part:** `Outpost::WindowManager` keeps the game's windows on a stack, and the places under the Ore that are the game's ([ADR-031](ADR-031-floating-windows.md) decision 1). `Hud::KeepOnScreen` keeps 120 units of a window's width.
5. **The look is a theme** (`Neuron::Theme`): the Style Guide's roles as one struct. It holds the colors of:
   - a window's body, its title bar, its close box and its corners;
   - a card, a pick and a field, and a disabled small button;
   - an edge, lit under the pointer and lit when picked;
   - the text, dim text, a label, a key, locked text and a figure;
   - the warning;
   - a bar's track and its fill.

   It also holds which font a window's title, its close box's mark, a name, a label and a figure take, the title's color, where it and the mark stand and how far the title is tracked, the corner's and the tick's sprites, and the sizes and gaps the widgets are drawn with, in reference units. The game fills it with its own values (`GameTheme`): a window's title is its header in the label face at the body's inset, as each window drew its own.
6. **The widgets draw into the display list what the HUD's helpers draw for the same thing** (`Neuron::Canvas`). Each is a node of the layout and what it draws there. The build records them in the order they are drawn, the HUD's first and then each window back to front. `Finish` arranges the layout and draws what was recorded.
   - **The widgets:**
     - a window (`BeginWindow`), a layer of its own with its frame, hatched title bar, close box and its mark, title, and a bracket at each corner, as `OpenWindow` draws them ([ADR-031](ADR-031-floating-windows.md) decision 3). It stands with its top-left corner at a place, or, until it is moved, at an anchor of the screen less a margin (decision 2), and its body stretches across it. Its title bar and its close box are regions that name nothing for the caller to do (`NO_PAYLOAD`), by which a press drags or closes it;
     - a panel of the HUD (`BeginPanel`), a body and its corners, as `Frame` draws it ([ADR-043](ADR-043-hud-in-the-windows-look.md) decision 1);
     - a box the game composes, its face, a hatch over it if it has one, and its edge, under what it holds, as `Painter::Panel` and `Painter::Outline` draw a card; and a card, a box that takes a press naming its payload while it has one. Neither is lit under the pointer;
     - a label, line by line;
     - an icon, a sprite of the atlas at its node, such as Ore's gem;
     - a button, as `AddButton` draws it ([ADR-043](ADR-043-hud-in-the-windows-look.md) decision 4). It is a card with its label cut short only where it meets its figure, the mark before its figure, its reason in its figure's place while it is disabled, or where a figure would stand on a button without one, its key's cap and a bar of the work under way. It is dim and takes no press while disabled, and its edge is lit under the pointer unless it is picked;
     - a small button and a stepper, `< SHIPYARD 05 >`, as `Painter::SmallButton` and `Painter::Stepped` draw them ([ADR-068](ADR-068-interface-polish.md) decision 1);
     - a row of cells with the pick lit, as `RetreatRow` draws it ([ADR-088](ADR-088-selection-and-commands.md) decision 3);
     - a checkbox, which no window draws yet;
     - a progress bar;
     - a chip, as `Painter::Chip` draws it ([ADR-085](ADR-085-one-meaning-per-color.md) decision 1);
     - a key cap ([ADR-068](ADR-068-interface-polish.md) decision 4);
     - a text field ([ADR-017](ADR-017-research-and-the-designer.md) decision 8), which says whether typing ended with Enter or with Esc, and whose text is in the warning's color while the caller refuses it.
   - **Places:** a place is drawn as reference units times the scale from the screen's corner, where `Painter` adds a window's corner and the place, each times the scale. The two differ only by the rounding of a float. Glyphs stay on whole pixels ([ADR-015](ADR-015-ui-drawing.md) decision 3).
   - **Only a chip fills the warning's color** ([ADR-085](ADR-085-one-meaning-per-color.md) decision 1). `Canvas::Fill`, which the game draws its own composites with, takes any color but that one, and fails on it (`MayFill`).
7. **The leave, away, Controls, orders and build windows are built on the canvas;** the HUD's panels, the menu, the designer, production and research are still placed by `Painter`, `OpenWindow`, `Frame` and `AddButton`, and all of it draws through the display list (decision 1).
   - **A canvas a window:** each of those windows is built on a canvas of its own, in the game's theme, and drawn into `Hud::Layout` where `OpenWindow` drew it, back to front. A button's payload is its action's place among the layout's actions, and the layout keeps the window's frame, title bar, close box, corner and first action as `OpenWindow` kept them, so that `ActionAt`, `HoverAt`, `WindowAt` and the drag read every window alike.
   - **What they hold:** the orders window's rows are steppers, and its sections stand in room for their most, so that it does not jump as the form changes. The build window's buttons are inset only by its padding.
   - **The pointer:** `GameClient` keeps one `Neuron::Interaction` and moves it each frame over the HUD drawn last, before it lays the next out. A button of a window on the canvas is lit while it is the hot widget, and no other with it; the HUD's own buttons are lit from the action under the pointer, as before, each whose action is that one.
   - **A button's reason:** a button whose label has a cost's place, after a `|`, shows its reason there while it is disabled, even with no cost, as Give does; one without shows none.
   - **Not yet:** there is no scroll view and no drop-down among the widgets. The regions that scroll, the overlay layer, the offsets and the open drop-down are what they will be built on.
8. **Every window is a row of one table** (`Outpost::WINDOWS`, `WindowTable.h`, in `GameApp`): which window it is; its name in the interface's file ([ADR-099](ADR-099-interface-settings-kept.md) decision 3); its title, on its title bar, which the designer has none of, its own standing in its body; the key that opens and closes it ([ADR-031](ADR-031-floating-windows.md) decision 6), which the away and leave windows have none of; where it first stands; whether `WindowsFit` counts it ([ADR-070](ADR-070-interface-scale.md) decision 3); and whether its place is kept from one session to the next ([ADR-099](ADR-099-interface-settings-kept.md) decision 1), which every window's is.
   - **Where a window first stands** (`FirstPlace`) is an anchor of the screen less a margin, so that a window not yet moved stands there on any screen and at any step, and never moves as the HUD's panels change:
     - the designer at the top-right corner;
     - production and research under the Ore, each in the first of the two places under the column's place that no open window held when it opened, which are the window manager's ([ADR-031](ADR-031-floating-windows.md) decisions 1 and 2);
     - the orders window at the right edge, level with those;
     - the Controls, away and leave windows in the middle;
     - the build window at the bottom right, beside the selection and its buttons where they leave it room at their widest, kept clear of the minimap as they are ([ADR-088](ADR-088-selection-and-commands.md) decision 2), and where they do not, at the right edge with its foot a gap above the minimap's top, so that it does not move as the selection changes.
   - **Fitting:** `WindowsFit` measures each window its row counts at its most: the designer as `ExtentOf` gives it, and a window on the canvas as it is built where nothing else is, with no font, since every line it holds is one line however wide. On the menu, with no match, it measures the Controls window alone.

## Consequences

- **`NeuronClientTests` tests the canvas without a GPU:**
  - `DrawListTests`: the layers, the regions and the clip.
  - `TextMetricsTests`: the measure.
  - `LayoutTreeTests`: columns, rows, grids and stacks with padding and gaps; least and most; an anchor to each corner, edge and the middle, and a node too large for its box at the box's start; text wrapped and cut, and a text of nothing a line tall; a height group, with a node measured and not drawn; a parent measured from its children; and the same tree at two scales.
  - `InteractionTests`: hot and active across frames, a press held through a move off and back, the front layer taking a press, the wheel over and off a scroll, focus and Esc's order, a drop-down closed by a press outside it, and an identifier claimed twice.
  - `WindowStackTests`: the order and a drag kept on the screen at each edge.
  - `CanvasTests`: each widget's display list in a theme of the tests' own, a window at an anchor, a window's body across its width, and a box, a card and an icon.
- **`HudTests` and `WindowManagerTests` hold the windows on the canvas as they held them before,** the build window's first place at 100% and 125% and the steps the windows fit among them.
- **A widget is a function object a frame,** an allocation each, until `Finish` draws it.
- **The canvas keeps no widget between frames:** only the interaction's state and the window stack last.

## What this forecloses

- A retained widget tree, which would have to be kept in step with the simulation: the canvas is built afresh every frame.
- Dear ImGui, RmlUi, Noesis or any other toolkit, without a new ADR (R14).
