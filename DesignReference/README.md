# Outpost Commander: Game Design Reference

This is an as-built design reference of the game at commit `a053fe294d91dc885bbb061c2f34d76ca6897738` (2026-10-25). It covers every rule, system, screen and asset, drawn and annotated. Every number and rule carries its source in this repository.

It is published as a Claude Design canvas: **https://claude.ai/artifact/GZ4mXfaSUXx1HrvZZ8XzXP**. It has 17 pages and 90 artboards.

This folder is documentation, not part of the game. Nothing in it is built, linked or shipped.

## Opening it in Claude Design

The reference already **is** a Claude Design canvas in your claude.ai account, so there is nothing to import. Open the link above.
- **Pages:** the page list on the canvas has one page per chapter, from 01 Cover to 17 Open items.
- **Navigation:** pan and zoom the canvas, and open any artboard to see it full size.
- **Comments:** comment on an artboard in place.
- **Sharing:** the canvas is private to you. To let someone else open it, use **Share** on the canvas page. They cannot open the link until you do.
- **Finding it again:** look in your artifact gallery at claude.ai/code/artifacts, or run `/artifacts` in the Claude Code terminal.

## Making a copy somewhere else

You might want the reference in another account or organization, or need to recreate it if the original is deleted. To do that, start a new session that can create Claude Design canvases. That can be Claude Code with this repository, or Claude Design on claude.ai with the pack attached. Paste [`CLAUDE_DESIGN_PROMPT.md`](CLAUDE_DESIGN_PROMPT.md) into it.

The prompt has Claude:
1. create a new Design canvas;
2. upload the 24 images;
3. re-point the artboards' image addresses;
4. publish the 90 artboards and the index unchanged.

Attach these when using Claude Design directly:
- `canvas/project/`: every file in it;
- `canvas/assets.json`;
- the 24 images it names, under `_work/assets/`.

## Regenerating it from the extractions

The canvas is generated, not hand-written, so a number reaches it without being retyped. To regenerate:

```
pip install --target DesignReference/compose/pylib markdown==3.7
python3 DesignReference/compose/build.py <root>                       # write the artboards, with local images
node DesignReference/compose/measure.cjs <root> heights.json          # measure each artboard's height (Playwright)
python3 DesignReference/compose/build.py <root> --assets DesignReference/compose/assets.json --heights heights.json
```

The last command writes `<root>/project/`, ready to publish. Use a fresh `assets.json` if you upload the images to a new canvas.

## What is here

| Path | What |
|---|---|
| `canvas/` | The canvas as published: `project/canvas.json`, the 90 `.dc.html` artboards, and `assets.json`. |
| `compose/` | The generator: `build.py` (chapters), `screens.py` (screen frames from layouts), `lib.py` (tokens, page look, markdown), `measure.cjs`. |
| `CLAUDE_DESIGN_PROMPT.md` | The prompt for rebuilding the canvas elsewhere. |
| `_work/manifest.json`, `_work/index.md` | The commit the reference describes, and the routing table of every design document, plan and ADR. |
| `_work/A1` … `_work/A7` | The seven extractions: rules and economy, units and combat, orders, AI and world, screen inventory (with `layouts/`, derived from `Hud.cpp`), visual language (with `tokens.json`), asset renders, and world presentation. |
| `_work/assets/` | Model renders (3/4 and top, 1024 and 512 px), contact and level sheets, textures and logos. They are approximations of the in-game look, rendered from the shipped `.nmf` meshes. |
| `_work/conflicts.md`, `gaps.md`, `glossary.md`, `R/` | The reconciliation: 68 conflicts, 79 gaps, 201 terms, the Horizon and self-play leak check, and the summary with the owner's answers. |
| `_work/V/` | The independent verification of the published canvas. |

## Limits

- **Nothing was built or run.** The container had no Windows, MSBuild or GPU.
- **Screen geometry is derived, not dumped.** It comes from `Hud.cpp`'s arithmetic. Any width that depends on DirectWrite text measurement is an estimate, and is marked as one.
- **Model images are approximations.** They are rendered in software from the shipped meshes and replay the game's shaders; they are not captured from the game.
- **Verification.** Two independent passes checked the canvas against the repository (`_work/V/V.md`, `V2.md`). V found 17 failing items. V2 found 16 of them fixed. The owner asked for the last one, and two defects V2 found, to be fixed as well (`_work/V/open-after-verification.md`).
- **Owner decisions.** The answers to the reference's open questions are in `_work/R/owner-decisions.md`. A draft plan that brings them to `main` is `_work/R/ImplementationPlan-Findings.draft.md`.

The final state is tagged `design-reference-2026-10-25`.
