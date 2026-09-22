#!/usr/bin/env python3
"""
build_meshes.py - OBJ -> CMO for the UWP client's Assets directory.

    python Scripts/build_meshes.py --obj Assets/Meshes/obj --out <ClientProject>/Assets/Meshes

Three stages, each of which fails loudly:

  1. meshconvert every .obj to .cmo
  2. re-inject the vertex colour selector from the .vcol sidecars (meshconvert writes white or zero)
  3. verify every .cmo against manifest.json - counts, extents, and the landmark tests

=== meshconvert is a TOOL in the build, not a linked library. ===
R14 survives on exactly that distinction. Do not add DirectXMesh as a dependency; do not vendor
its source. Pin the version below and make the failure say which version it wanted, because this
is the step that breaks on somebody else's machine.
"""

import argparse, json, os, struct, subprocess, sys

# --- PINNED. Record the exact release you tested against; do not float this. -------------------
MESHCONVERT_RELEASE = "TODO: pin the DirectXMesh release tag or commit sha you tested against"
MESHCONVERT_EXE = os.environ.get("MESHCONVERT", "meshconvert.exe")

# === THE HANDEDNESS FLAG IS AN OPEN QUESTION - DETERMINE IT, DO NOT GUESS. =====================
# The handoff authors left-handed, Y up, +Z forward. OBJ is conventionally right-handed. Whether
# meshconvert needs a flip flag for this input is NOT settled in the handoff, and an earlier draft
# of the build prompt asserted "do not flip anything" with more confidence than was warranted.
#
# Resolve it empirically, once: run stage 1 with MESHCONVERT_FLAGS as-is, run stage 3, and read the
# landmark failures. They are designed to name the axis that is wrong:
#   Frigate nose not at max +Z            -> Z is flipped
#   OreProcessorL2 large drum at +X       -> X is mirrored
#   Station hub tower not at max +Y       -> Y is flipped
# Then set the flags, record WHY in this comment, and never revisit it.
MESHCONVERT_FLAGS = ["-cmo", "-nodds", "-y"]   # -y = overwrite existing output


def fail(msg, code=1):
    sys.stderr.write("build_meshes: %s\n" % msg)
    sys.exit(code)


def load_vcol(path):
    out = []
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) != 4:
                fail("%s: expected 4 values per line, got %r" % (path, line))
            out.append(tuple(int(x) for x in parts))
    return out


def convert(obj_dir, out_dir, names):
    os.makedirs(out_dir, exist_ok=True)
    for name in names:
        obj = os.path.join(obj_dir, name + ".obj")
        if not os.path.isfile(obj):
            fail("missing %s - run meshes_to_obj.py first" % obj)
        cmd = [MESHCONVERT_EXE] + MESHCONVERT_FLAGS + ["-o", os.path.join(out_dir, name + ".cmo"), obj]
        try:
            r = subprocess.run(cmd, capture_output=True, text=True)
        except FileNotFoundError:
            fail("%s not found on PATH.\n"
                 "  Set the MESHCONVERT environment variable to its full path.\n"
                 "  Required release: %s" % (MESHCONVERT_EXE, MESHCONVERT_RELEASE), code=2)
        if r.returncode != 0:
            fail("meshconvert failed on %s (exit %d)\n%s\n%s"
                 % (name, r.returncode, r.stdout, r.stderr))
        print("  converted %s" % name)


def inject_vertex_colours(out_dir, obj_dir, names):
    """meshconvert has no vertex-colour input from OBJ, so it writes white or zero into the colour
    DWORD. Patch it from the sidecars.

    This walks the CMO vertex array by stride. Implement it against the format, not against these
    files: read the submesh/vertex-buffer header, seek to the vertex array, and write the colour
    DWORD at (vertexBase + i*stride + colourOffset). Assert the vertex count you find equals the
    sidecar length BEFORE writing anything - a mismatch means meshconvert welded or reordered,
    which invalidates the whole face-split contract and must abort the build."""
    for name in names:
        vcol = load_vcol(os.path.join(obj_dir, name + ".vcol"))
        cmo = os.path.join(out_dir, name + ".cmo")
        # TODO(implementer): patch the colour DWORD in-place. Contract:
        #   - CMO vertex stride is 52 bytes: pos f3 (0), normal f3 (12), tangent f4 (24),
        #     colour uint32 (40), texcoord f2 (44)
        #   - colour packs as B8G8R8A8 or R8G8B8A8 depending on the reader you write; pick one,
        #     assert it in CmoReader's unit test, and keep the two in sync
        #   - abort if the CMO vertex count != len(vcol)
        raise SystemExit(
            "build_meshes: vertex-colour injection for %s is not implemented yet.\n"
            "  %d vertices are waiting in %s.vcol.\n"
            "  This is deliberate: shipping meshes with a white colour channel would look like a\n"
            "  shader bug for a week. Implement the patch, then delete this guard."
            % (name, len(vcol), name))


def verify(out_dir, manifest):
    """Assert counts, extents and the landmark tests. Extents are asserted in WORLD UNITS with no
    scale factor - the authored extent IS the object's size."""
    problems = []
    for m in manifest["meshes"]:
        cmo = os.path.join(out_dir, m["name"] + ".cmo")
        if not os.path.isfile(cmo):
            problems.append("%s: missing" % m["name"])
            continue
        # TODO(implementer): read the .cmo back and compare
        #   vertex count == m["vertices"], triangle count == m["triangles"],
        #   bounding box == m["extents"]["min"] / ["max"] within 0.01
        # Then run manifest["landmarkTests"] - they name the axis that is wrong when they fail.
    if problems:
        fail("verification failed:\n  " + "\n  ".join(problems))
    print("build_meshes: verification not implemented - see verify_cmo.py")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--obj", default="Assets/Meshes/obj")
    ap.add_argument("--out", required=True, help="the UWP client's Assets/Meshes directory")
    ap.add_argument("--manifest", default="design_handoff_meshes/integration/manifest.json")
    ap.add_argument("--skip-convert", action="store_true")
    args = ap.parse_args()

    with open(args.manifest, "r", encoding="utf-8") as fh:
        manifest = json.load(fh)
    names = [m["name"] for m in manifest["meshes"]]

    print("build_meshes: %d meshes -> %s" % (len(names), args.out))
    if not args.skip_convert:
        convert(args.obj, args.out, names)
    inject_vertex_colours(args.out, args.obj, names)
    verify(args.out, manifest)

    total = sum(m["cmoBytesEstimate"] for m in manifest["meshes"])
    print("build_meshes: done. %d files, ~%d KiB of CMO (pre-appx-compression)."
          % (len(names), total // 1024))


if __name__ == "__main__":
    main()
