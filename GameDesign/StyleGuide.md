# Outpost Commander — Style Guide

Status: **living** · The rules a new element of the screen obeys, in the HUD, in a window or in the world, each with the ADR decision that owns it

A rule here names its constant and its ADR decision and gives no value: the colors are listed in [Reference/Colors.md](Reference/Colors.md), and the sizes are the code's. What a player meets of these rules is [the living design](OutpostCommander.md)'s, at Game §interface.style and Game §look. A new element that needs a rule this guide does not have needs a decision first, recorded in the ADR that owns its subject.

---

## Color

- **One color has one meaning.** In the HUD and on the minimap, whose a thing is, is its color: the player's own blue (`OWN_COLOR`), the enemy's red (`ENEMY_COLOR`) and the pirates' violet (`PIRATE_COLOR`); Ore's gold (`GOLD_COLOR`) is for ore alone ([ADR-085](../Design/ADR/ADR-085-one-meaning-per-color.md), [ADR-043](../Design/ADR/ADR-043-hud-in-the-windows-look.md) decision 7, [ADR-073](../Design/ADR/ADR-073-pirates.md) decision 10).
- **In the world, a side's models are drawn in its set's color,** which `Models.json` gives each seat, and a structure in that color taken toward gray ([ADR-011](../Design/ADR/ADR-011-meshes-and-shading.md) decision 3, [ADR-040](../Design/ADR/ADR-040-lines-over-dark-faces.md) decision 4).
- **Nothing that is no side's takes a side's color:** a blast runs from white-yellow to amber ([ADR-026](../Design/ADR/ADR-026-explosions-and-particles.md) decision 8), tracers, flashes and sparks are yellow ([ADR-034](../Design/ADR/ADR-034-shot-looks.md) decision 4), and a drive's exhaust is none of the sides' colors ([ADR-019](../Design/ADR/ADR-019-glows-and-exhaust.md) decision 3).
- **A beam is its shooter's side's color, lightened toward white,** so a beam says who fires it ([ADR-034](../Design/ADR/ADR-034-shot-looks.md) decision 4).
- **The sky's crosses are never warm,** so no star reads as a marker in Ore's gold ([ADR-028](../Design/ADR/ADR-028-vector-grid-and-crosses.md) decision 3).
- **Ore is written one way everywhere:** its gem in `GOLD_COLOR` and its figure grouped in thousands; on a button that cannot be pressed, both take the locked text's color ([ADR-043](../Design/ADR/ADR-043-hud-in-the-windows-look.md) decision 3, [ADR-085](../Design/ADR/ADR-085-one-meaning-per-color.md) decision 3).
- **A warning that stands where the enemy's figures do is a chip,** its words in `WINDOW_COLOR` on a filled tag of `WARNING_COLOR` drawn by `Painter::Chip`, and nothing but a warning stands on such a tag ([ADR-085](../Design/ADR/ADR-085-one-meaning-per-color.md) decision 1).
- **A warning in plain text in `WARNING_COLOR` stands only where no enemy figure does** ([ADR-085](../Design/ADR/ADR-085-one-meaning-per-color.md) decision 1).
- **A chip marks only what the player could act on now;** a state whose remedy lies elsewhere is plain text ([ADR-066](../Design/ADR/ADR-066-production-status-on-the-hud.md) decision 2).
- **Progress that belongs to no side is neutral gray, and the HUD's own progress is blue:** the build bar over any side's structure is `BUILD_BAR_COLOR`, and a bar on the player's own buttons, queues and status lines is `BAR_FILL_COLOR` on `BAR_TRACK_COLOR` ([ADR-085](../Design/ADR/ADR-085-one-meaning-per-color.md) decision 2, [ADR-066](../Design/ADR/ADR-066-production-status-on-the-hud.md) decision 3).
- **Every health bar fills from one set of colors,** green, amber and red by the share of hit points left (`HEALTH_GOOD_COLOR`, `HEALTH_FAIR_COLOR`, `HEALTH_POOR_COLOR`), on a back of its side's color; the figures, the designer's changes and the ratings keep the figures' own set ([ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 5).
- **A mark tells apart what it is by shape and size, not by color alone:** on the minimap a ship is a small square, a structure a larger one and an ore asteroid an outline ([ADR-043](../Design/ADR/ADR-043-hud-in-the-windows-look.md) decisions 7 and 8), and a production card's ratings read without their colors ([ADR-068](../Design/ADR/ADR-068-interface-polish.md) decision 3).
- **A sector's state is a pattern, not a color:** dashed for suppressed and dotted for cut off in the world, dashed and hatched on the minimap ([ADR-081](../Design/ADR/ADR-081-territory-and-memory-over-the-fog.md) decisions 3 and 7).
- **What the player only remembers is drawn as a memory:** its lines alone, in its side's color taken toward gray, with no faces, bar or ring, and a cross on the minimap ([ADR-081](../Design/ADR/ADR-081-territory-and-memory-over-the-fog.md) decision 4).

## Contrast

- **Every text stands at 4.5:1 or more against what it is drawn on,** WCAG's ratio of the linear colors, a hatched panel's stripes and the gaps between them both counted; a new text color is set for that floor, and `HudTests.SetsEveryTextAtFourAndAHalfToOne` holds it ([ADR-062](../Design/ADR/ADR-062-type-scale-and-contrast.md) decision 3).
- **A chip's words meet the same floor on its tag** ([ADR-085](../Design/ADR/ADR-085-one-meaning-per-color.md) decision 1, [ADR-062](../Design/ADR/ADR-062-type-scale-and-contrast.md) decision 3).
- **A minimap mark is set apart from the map's ground by a ratio computed as the ore's and the fields' are, and from the enemy's filled marks by its shape as well as its brightness** ([ADR-043](../Design/ADR/ADR-043-hud-in-the-windows-look.md) decision 8).
- **A health bar's colors are chosen against the backs of every side's color,** not against a window's navy ([ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 5).
- **A model's lines stand above its faces on every face the camera can see,** at the floors `SceneShadingTests.KeepsEveryModelsLinesOverItsFaces` holds for ships, structures and rocks (`SHIP_FLOOR`, `STRUCTURE_FLOOR`, `ROCK_FLOOR`); a new set, a new light or a new shade keeps them ([ADR-040](../Design/ADR/ADR-040-lines-over-dark-faces.md) decision 1, [ADR-090](../Design/ADR/ADR-090-a-warm-key-and-a-cool-fill.md) decision 3).
- **A rock's lines stand out from its faces but never outshine a ship's or a structure's,** and the terrain stands back from the fleet ([ADR-040](../Design/ADR/ADR-040-lines-over-dark-faces.md) decision 1).
- **A structure stands back from the ships:** it is never as bright or as saturated as its side's ships ([ADR-040](../Design/ADR/ADR-040-lines-over-dark-faces.md) decision 4).

## Text

- **A text takes one of the faces `Hud::Typeface` names,** each the first installed family of its list; the game ships no font ([ADR-030](../Design/ADR/ADR-030-typography-and-sprites.md) decisions 1 and 2).
- **A face that is not installed fails;** nothing falls back to whatever the system offers ([ADR-030](../Design/ADR/ADR-030-typography-and-sprites.md) decision 2).
- **A face's size is `Hud::FACE_UNITS`', read through `Hud::FaceUnits`,** and no call passes a size of its own ([ADR-062](../Design/ADR/ADR-062-type-scale-and-contrast.md) decisions 1 and 2).
- **A panel's first line is in the title face, its other lines in the name face and its figures in the figure face** ([ADR-043](../Design/ADR/ADR-043-hud-in-the-windows-look.md) decision 2).
- **Labels are small capitals spaced by a tracking,** passed to `DrawText` and `TextWidth` alike ([ADR-030](../Design/ADR/ADR-030-typography-and-sprites.md) decision 4).
- **Text is printable ASCII, the middle dot and the multiplication sign;** a character the interface needs beyond them is added to `GlyphAtlas::EXTRA_CHARACTERS` first, or written in ASCII, as a hyphen for a minus ([ADR-030](../Design/ADR/ADR-030-typography-and-sprites.md) decision 3, [ADR-057](../Design/ADR/ADR-057-domination.md) decision 8).
- **A width that matters is measured with the face it is drawn in,** through `Hud::TextMetrics`, never counted in characters or taken as a share of a face's size ([ADR-061](../Design/ADR/ADR-061-measured-text.md) decisions 1 and 3).
- **A line that could run past what holds it is cut short with three dots** by `TextMetrics::Fit`, leaving `FIT_GAP_UNITS` from what stands beside it, and text that wraps is broken between words by `TextMetrics::Wrap` ([ADR-061](../Design/ADR/ADR-061-measured-text.md) decision 3).
- **Lines are spaced for the rounding of every scale,** since each face is rasterized at a whole number of pixels ([ADR-062](../Design/ADR/ADR-062-type-scale-and-contrast.md) decision 5, [ADR-061](../Design/ADR/ADR-061-measured-text.md) decision 4).
- **A panel or a tag whose text changes is as wide as its widest fixed text,** so that its edge never moves as the text changes ([ADR-066](../Design/ADR/ADR-066-production-status-on-the-hud.md) decision 3, [ADR-086](../Design/ADR/ADR-086-where-the-match-runs.md) decision 2).
- **Every new text is in the HUD tests' longest content,** so that `HudTests.KeepsEveryTextInsideItsPanel` and `HudTests.OverlapsNoTwoTexts` lay it out at both of their screen sizes ([ADR-061](../Design/ADR/ADR-061-measured-text.md) decision 5).

## Panels, buttons and windows

- **The interface is laid out in reference units on ADR-006's reference frame,** each element anchored to a corner, an edge or the middle, and never in back-buffer pixels ([ADR-006](../Design/ADR/ADR-006-renderer-shape.md) decision 5, [ADR-015](../Design/ADR/ADR-015-ui-drawing.md) decision 4).
- **The layout holds no GPU state and no camera;** what is placed from the world, such as a bar's foot, is handed to it ([ADR-015](../Design/ADR/ADR-015-ui-drawing.md) decision 4, [ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 5).
- **The interface is one draw call of quads,** text, panels, hatching, sprites and lines alike, within `UiPipeline::MAX_QUADS` a frame ([ADR-015](../Design/ADR/ADR-015-ui-drawing.md) decision 2).
- **A sprite is drawn into the atlas on the CPU, not shipped,** and listed in `Hud::Sprites` ([ADR-030](../Design/ADR/ADR-030-typography-and-sprites.md) decision 5).
- **Hatching is drawn by the pixel shader with `FillHatched`,** so neighboring hatched rectangles line up ([ADR-030](../Design/ADR/ADR-030-typography-and-sprites.md) decision 6).
- **Every panel of the HUD takes a window's look,** a window's body and a bracket at each corner, without a title bar ([ADR-043](../Design/ADR/ADR-043-hud-in-the-windows-look.md) decision 1).
- **A window is a frame, a hatched title bar, a close box and four corner brackets,** with an opaque body ([ADR-031](../Design/ADR/ADR-031-floating-windows.md) decision 3, [ADR-068](../Design/ADR/ADR-068-interface-polish.md) decision 1).
- **A window's state is `WindowManager`'s,** its place kept in reference units until the game closes, and `Hud::KeepOnScreen` keeps its title bar on the screen ([ADR-031](../Design/ADR/ADR-031-floating-windows.md) decisions 1 and 2).
- **The HUD and the windows are drawn and clicked in layers,** so a window covers the text behind it and only the front layer takes a click ([ADR-031](../Design/ADR/ADR-031-floating-windows.md) decision 4).
- **A press on a panel or a window is the HUD's,** and a release always reaches the player's controls ([ADR-015](../Design/ADR/ADR-015-ui-drawing.md) decision 5).
- **A structure's window opens from a button on its selection panel and from its key, which also closes it** ([ADR-031](../Design/ADR/ADR-031-floating-windows.md) decision 6).
- **Every key the game reads in a match is a constant in `GameApp/KeyBindings.h`,** and the Controls window lists it from there ([ADR-068](../Design/ADR/ADR-068-interface-polish.md) decision 4).
- **A button is a card:** a card's face and edge, its label in the name face, and any cost as Ore's gem and figure at its right ([ADR-043](../Design/ADR/ADR-043-hud-in-the-windows-look.md) decision 4).
- **A button that cannot be pressed is dim and takes no click;** when the reason is not its cost, it says why in place of the cost ([ADR-043](../Design/ADR/ADR-043-hud-in-the-windows-look.md) decision 4, [ADR-015](../Design/ADR/ADR-015-ui-drawing.md) decision 4).
- **A button does what its key does, and shows its key's cap** ([ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 2).
- **What the pointer is over lights its edge, and nothing else changes:** no word, place or click ([ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 1).
- **An armed order's button is lit as a pick** until the order is given or canceled ([ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 2).
- **A choice among a few settings is a row of cells, the current one lit as a pick,** so the player sees every choice and picks one in a press ([ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 3).
- **A standing state is a status line, and a place to click** ([ADR-066](../Design/ADR/ADR-066-production-status-on-the-hud.md) decisions 1 and 5).
- **An event is an alert,** raised from the server's events, listed under the territory panel and marked on the minimap ([ADR-059](../Design/ADR/ADR-059-alerts-and-standing-orders.md) decisions 1 and 2).

## The interface's scale

- **The player's factor multiplies the screen's fit,** and the HUD, the windows and the menu are laid out, measured and hit-tested at it ([ADR-070](../Design/ADR/ADR-070-interface-scale.md) decision 1).
- **A window of a fixed size is counted by `Hud::WindowsFit`,** so that no step makes it taller than the screen ([ADR-070](../Design/ADR/ADR-070-interface-scale.md) decision 3).
- **The atlas is rasterized again when the scale moves a font or a sprite by a whole pixel,** and glyphs stand on whole pixels ([ADR-015](../Design/ADR/ADR-015-ui-drawing.md) decision 3, [ADR-070](../Design/ADR/ADR-070-interface-scale.md) decision 6).
- **A mark in the world keeps the screen's own scale, not the player's factor:** the stars, the bars over the entities and the selection's ring ([ADR-070](../Design/ADR/ADR-070-interface-scale.md) decision 7).

## Marks in the world

- **A bar over an entity is `Hud::BarOver`'s,** its foot from `Camera::PixelAbove` over its model's bounds, on whole pixels, drawn under every panel and window and taking no click ([ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 5).
- **A line on the ground is one pixel wide at any zoom:** a footprint ring, a sector's border and the grid ([ADR-042](../Design/ADR/ADR-042-faint-footprint-lines.md) decision 1, [ADR-081](../Design/ADR/ADR-081-territory-and-memory-over-the-fog.md) decision 3, [ADR-028](../Design/ADR/ADR-028-vector-grid-and-crosses.md) decision 1).
- **The selection's ring is a set width in reference units at any zoom,** and the strongest mark on the ground ([ADR-067](../Design/ADR/ADR-067-selection-ring-at-a-fixed-width.md) decision 1, [ADR-042](../Design/ADR/ADR-042-faint-footprint-lines.md) decision 3).
- **A resting ring is for the far view:** it fades up close, and shows at full strength only under the pointer and while a structure is placed ([ADR-042](../Design/ADR/ADR-042-faint-footprint-lines.md) decisions 1 and 2).
- **A sector's border is brighter than the grid and never wider** ([ADR-081](../Design/ADR/ADR-081-territory-and-memory-over-the-fog.md) decision 3).
- **The territory and what the player remembers are drawn over the fog,** since the fog does not hide them ([ADR-081](../Design/ADR/ADR-081-territory-and-memory-over-the-fog.md) decisions 3 and 4).
- **A mark is shown only while it says something:** a node's rings only while a Relay is placed, and a health bar only while its entity is hurt or Alt is held ([ADR-081](../Design/ADR/ADR-081-territory-and-memory-over-the-fog.md) decision 3, [ADR-088](../Design/ADR/ADR-088-selection-and-commands.md) decision 5).
- **An effect stands at one height above the ground,** above the ships ([ADR-019](../Design/ADR/ADR-019-glows-and-exhaust.md) decision 5).

## The scene

- **Faces are dark and lines bright, and the lines keep their hue;** a new model's colors go through `SceneShading.h`'s `Shaded` and `EdgeColor` ([ADR-040](../Design/ADR/ADR-040-lines-over-dark-faces.md) decision 1, [ADR-090](../Design/ADR/ADR-090-a-warm-key-and-a-cool-fill.md) decision 4).
- **A line is pulled toward the eye by its instance's `liftShare`,** never lifted along its normal ([ADR-040](../Design/ADR/ADR-040-lines-over-dark-faces.md) decision 2).
- **Lines are one pixel wide, test depth and write none,** and are drawn after every face ([ADR-027](../Design/ADR/ADR-027-rock-crease-lines.md) decision 3).
- **The scene is drawn at four samples a pixel and resolved by a shader,** and the interface is drawn after the resolve at one sample ([ADR-040](../Design/ADR/ADR-040-lines-over-dark-faces.md) decision 3, [ADR-050](../Design/ADR/ADR-050-shader-resolve.md) decisions 1 and 2).
- **A face is lit by `SceneShading.h`'s key and fill,** and `FaceLight` works the light out as the shaders do, so that a test reads what the game draws ([ADR-090](../Design/ADR/ADR-090-a-warm-key-and-a-cool-fill.md) decisions 1 and 4).
- **A line takes only the light's luminance,** so a ridge keeps its side's hue and the grid, the borders and the rings keep their colors ([ADR-090](../Design/ADR/ADR-090-a-warm-key-and-a-cool-fill.md) decision 2).
- **No open face the camera can see is lit below the ambient floor,** and a mark that lies flat on the ground keeps its color ([ADR-090](../Design/ADR/ADR-090-a-warm-key-and-a-cool-fill.md) decision 3).
- **Occlusion darkens faces only, never lines,** and a vertex made on the CPU is wholly open ([ADR-092](../Design/ADR/ADR-092-baked-occlusion.md) decisions 3 and 4).
- **Occlusion is baked by `Tools/BakeMeshes.py`,** never painted by hand ([ADR-092](../Design/ADR/ADR-092-baked-occlusion.md) decision 1).
- **What glows is what is bright:** the glow takes the scene's light above `SCENE_BLOOM`'s threshold, and nothing marks what should glow ([ADR-091](../Design/ADR/ADR-091-a-phosphor-glow.md) decisions 1 and 3).
- **The interface never glows,** since it is drawn after the resolve that adds the glow ([ADR-091](../Design/ADR/ADR-091-a-phosphor-glow.md) decision 1).
