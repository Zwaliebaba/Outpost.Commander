# Prompt: rebuild the Outpost Commander Game Design Reference as a Claude Design canvas

Paste everything below the line into a new Claude session that can create Claude Design canvases. Two cases:
- **Claude Code** with this repository checked out: nothing to attach.
- **Claude Design on claude.ai:** attach the files listed under "The pack".

The original canvas already exists at https://claude.ai/artifact/GZ4mXfaSUXx1HrvZZ8XzXP. You only need this prompt to make a copy elsewhere: in another account, another organization, or after the original is deleted.

---

Rebuild a Claude Design canvas from a finished pack. Do not redesign, re-research or reword anything: every artboard is final HTML in the Design canvas format (`.dc.html`). Your job is to recreate the canvas and re-point its images.

**The pack** is the folder `DesignReference/` of the repository `Zwaliebaba/Outpost.Commander`, or the same files attached to this message:
- `canvas/project/canvas.json`: the canvas index, with 17 pages, 90 artboards, their frames and the page titles.
- `canvas/project/*.dc.html`: the 90 artboards, one file each, complete.
- `canvas/assets.json`: a map from each image's pack path to the upload address it had on the original canvas (`/_blob/<id>`).
- The 24 images that map names, under `_work/assets/` (`sheets/`, `models/`, `textures/`, `logos/`, `mockup/`).

**Steps:**
1. Create a new canvas from the Design artifact type. Title it `Outpost Commander — Game Design Reference`.
2. Upload the 24 images in `canvas/assets.json` to the new canvas's asset store. Each upload returns a new `/_blob/<id>` address.
3. In every `.dc.html`, replace each old `/_blob/<id>` from `canvas/assets.json` with the new address for the same image. Change nothing else in the files.
4. Publish `project/canvas.json` first, with `Main.dc.html`. Then publish the other 89 artboards at the same paths. Keep `canvas.json` as it is, except its `createdOnFiles.at`, which is now.
5. Do not verify, re-render or "improve" the artboards. They were measured and verified when they were made (`DesignReference/_work/V/V.md`).
6. Reply with the new canvas's link.

**What the reference is,** if you are asked:
- It is an as-built design reference of the game at commit `a053fe294d91dc885bbb061c2f34d76ca6897738` (2026-10-25).
- It covers rules, economy, units, research, combat, orders, AI, the persistent world, the visual language, every screen state drawn at 1920×1080, the battlefield, an asset catalogue, controls and the open conflicts.
- Every number cites its source in the repository.
