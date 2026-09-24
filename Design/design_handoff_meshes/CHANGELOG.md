# Changelog — Outpost Commander MVP meshes

## v4 — 2026-09-24
- **Station redesigned** (500 → 1,032 tris). Tiered 12-sided core with command crown, antenna mast and keel
  spire; four slim arms ending in open docking cradles; habitat ring above the arms on four spokes; twin-barrel
  point-defence turrets (E/W); radiator fins (N/S). Height 54 → 86 units (−26 … +60). **Landmark test updated
  to 220 × 86 × 220** — any copy of `verify_cmo.py` from v3 or earlier will fail on this Station.
- Package re-laid out to mirror the repo (`Assets/`, `Scripts/`, `Client/`, `Design/`); scripts now resolve
  their defaults relative to the package, so they run from wherever it is unpacked.
- `PACKAGE.json` added: per-file SHA-256 and byte size.

## v3 — 2026-09-24
- **Cruiser added** on the owner's instruction, overriding the brief. 180 × 64 units, 316 tris, hammerhead
  slab with four dorsal turrets. Length **not echoed from the brief — confirm against the catalog.**
- New Cruiser landmark test (prow vs tail width). 13 → 14 meshes and instanced draws.

## v2 — 2026-09-22
- Triangle budget withdrawn; every mesh re-authored to what its form needs (1,002 → 2,674 tris).
- Shipyard rebuilt as an open lattice spine (members 3–5 units on 18-unit bays).
- Integration package: convert-ready OBJ + VCOL, asset manifest, UWP content declarations, scripts.
- Withdrawn: the claim that `meshconvert` needs no flip flag. Now an empirical check with landmark tests.

## v1 — 2026-09-22
- First handoff: 13 meshes, plates, verdict that geometry alone fails the 3.5 px gate, overlay fallback.
