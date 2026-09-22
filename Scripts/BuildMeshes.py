#!/usr/bin/env python3
"""
build_meshes.py - OBJ -> CMO for the UWP client's Assets directory.

    python Scripts/BuildMeshes.py

Three stages, each of which fails loudly:

  1. meshconvert every .obj to .cmo
  2. re-inject the vertex colour selector from the .vcol sidecars (meshconvert writes white or zero)
  3. verify every .cmo against manifest.json - counts, extents, and the landmark tests

=== meshconvert is a TOOL in the build, not a linked library. ===
R14 survives on exactly that distinction. Do not add DirectXMesh as a dependency; do not vendor
its source. Pin the version below and make the failure say which version it wanted, because this
is the step that breaks on somebody else's machine.
"""

import argparse, hashlib, json, os, struct, subprocess, sys

# --- WHERE THINGS ARE IN *THIS* TREE ------------------------------------------------------------
# The handoff's scripts defaulted to a generic layout -- `Assets/Meshes` at the repository root and
# the manifest still inside the handoff folder. Here the package project owns the assets:
# `OutpostCommander/Assets/`, which AGENTS.md section 2 names as the one subdirectory in the tree.
# The manifest ships beside the meshes so the runtime catalog asserts against the same file the
# build validated against.
#
# Resolved from this file's own location rather than from the working directory, so every one of
# these runs the same from anywhere -- including from a build step whose working directory is a
# project folder.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OBJ_DIR = os.path.join(REPO_ROOT, "OutpostCommander", "Assets", "Meshes", "obj")
DEFAULT_OUT_DIR = os.path.join(REPO_ROOT, "OutpostCommander", "Assets", "Meshes")
DEFAULT_MANIFEST = os.path.join(REPO_ROOT, "OutpostCommander", "Assets", "Meshes", "manifest.json")
DEFAULT_MESHES_JSON = os.path.join(REPO_ROOT, "Design", "design_handoff_meshes", "meshes.json")

# --- PINNED, AND PINNED TWICE. ------------------------------------------------------------------
# The version is what meshconvert prints in its own banner. The HASH is what makes the pin mean
# anything: a release tag says which source was built, a hash says which BINARY is sitting in
# Tools/ -- and it is the binary that produces the bytes we ship. The handoff warns that this is
# the step which breaks on somebody else's machine, so the failure below names both.
#
# Recorded 2026-09-22 from the tool as delivered. To move it: replace both, run
# `python Scripts/BuildMeshes.py`, and confirm CheckMeshes.py is still clean on the CMO stage. A
# different converter is a different set of bytes until something proves otherwise.
MESHCONVERT_VERSION = "2026.5.8.1"
MESHCONVERT_SHA256 = "b659ac07784f85e3ca4645f7dc9a0290492c960bd75c83035215c3e80692f950"

# IN Tools/, NOT ON PATH. R14 survives on meshconvert being a tool in the build rather than a
# linked library, and a tool in the build is one this repository can point at -- not one every
# machine has to install the same way. MESHCONVERT overrides it for a machine that keeps its tools
# somewhere else.
MESHCONVERT_EXE = os.environ.get("MESHCONVERT", os.path.join(REPO_ROOT, "Tools", "meshconvert.exe"))

# === HANDEDNESS: RESOLVED EMPIRICALLY 2026-09-22, AND HERE IS WHAT RESOLVED IT. =================
# No flip flag. Not asserted -- measured, on output this tool produced.
#
# `Scout` converted and its CMO extents block came back min (-27, -8, -30) max (27, 10, 30), which
# is the manifest's 54 x 18 x 60 exactly and in the same orientation. **Y is the axis that proves
# it**: the hull is asymmetric about the plane, -8 below and +10 above, so a Y flip would have come
# back as (-10, +8) and it did not. X and Z are symmetric on this hull and prove nothing by
# themselves -- the Frigate nose and the ShipyardL1 truss landmarks in manifest.json are what close
# those two, and CheckMeshes.py runs them on the decoded positions.
#
# So the handoff's left-handed, Y up, +Z forward IS meshconvert's own CMO convention, which is what
# the withdrawn first draft of the prompt claimed without evidence. It happens to have been right.
#
# THE SYNTAX MOVED. The handoff passes `-cmo`; this version documents `-ft cmo` and takes the old
# spelling as a legacy alias. The documented form is used here so a future version dropping the
# alias fails loudly at the flag rather than quietly at the format.
MESHCONVERT_FLAGS = ["-ft", "cmo", "-nodds", "-y"]   # -y = overwrite existing output


# --- THE CMO WALK -------------------------------------------------------------------------------
# Enough of the format to find the vertex array, and not one field more. This is the build's copy;
# the client's is NeuronClient/CmoReader, and the two are kept honest by both asserting the same
# counts and extents out of manifest.json rather than by reading each other.
#
# THE LAYOUT BELOW WAS OBSERVED, NOT RECALLED. It was checked field by field against a file this
# meshconvert produced, parsing to exactly zero trailing bytes. One thing in it is easy to get
# wrong from memory and did not survive contact: the material's ambient, diffuse, specular and
# emissive are float4, not float3, with the specular power a single float between specular and
# emissive. A float3 reading walks off the end inside the texture slots.
#
#   UINT      mesh count
#   per mesh:
#     wstr    name
#     UINT    material count
#     per material:
#       wstr  name
#       float4 ambient, float4 diffuse, float4 specular, float specularPower, float4 emissive
#       float4x4 uv transform
#       wstr  pixel shader name          (non-empty here: "lambert.dgsl" / "phong.dgsl")
#       wstr  x 8  texture file names     (all empty here, each still length-prefixed)
#     BYTE    skeletal animation present
#     UINT    submesh count, then 5 x UINT each
#     UINT    index buffer count, then per buffer: UINT index count, uint16 x count
#     UINT    vertex buffer count, then per buffer: UINT vertex count, 52 bytes x count
#     UINT    skinning vertex buffer count
#     float   center[3], radius, min[3], max[3]
#     (bones and animation clips follow when the skeletal byte is set)
#
# A wstr is a UINT count of UTF-16 code units followed by that many, the terminating null included.

VERTEX_STRIDE = 52
OFFSET_POSITION = 0
OFFSET_NORMAL = 12
OFFSET_TANGENT = 24
OFFSET_COLOUR = 40
OFFSET_TEXCOORD = 44


class CmoWalk(object):
    def __init__(self, data):
        self.d = data
        self.o = 0

    def u32(self):
        v = struct.unpack_from("<I", self.d, self.o)[0]
        self.o += 4
        return v

    def u8(self):
        v = self.d[self.o]
        self.o += 1
        return v

    def skip_wstr(self):
        n = self.u32()
        self.o += n * 2

    def skip(self, n):
        self.o += n

    def floats(self, n):
        v = struct.unpack_from("<%df" % n, self.d, self.o)
        self.o += 4 * n
        return v


def walk_cmo(data, name):
    """Returns (vertex_offset, vertex_count, index_count, extents_min, extents_max)."""
    w = CmoWalk(data)

    n_meshes = w.u32()
    if n_meshes != 1:
        fail("%s: %d meshes in one file; the handoff is one submesh per file" % (name, n_meshes))
    w.skip_wstr()

    for _ in range(w.u32()):                      # materials
        w.skip_wstr()                             # material name
        w.skip(4 * (4 + 4 + 4 + 1 + 4))           # ambient, diffuse, specular, power, emissive
        w.skip(4 * 16)                            # uv transform
        w.skip_wstr()                             # pixel shader name
        for _ in range(8):                        # eight texture slots, empty but still present
            w.skip_wstr()

    skeletal = w.u8()
    if skeletal:
        # Not an error in the format -- an error in THIS content. Nothing here is skinned, and a
        # file that says it is means something upstream changed.
        fail("%s: the skeletal-animation byte is set; nothing in this handoff is skinned" % name)

    for _ in range(w.u32()):                      # submeshes
        w.skip(20)

    index_count = 0
    for _ in range(w.u32()):                      # index buffers
        n = w.u32()
        index_count += n
        w.skip(2 * n)

    n_vb = w.u32()
    if n_vb != 1:
        fail("%s: %d vertex buffers; expected exactly one" % (name, n_vb))
    vertex_count = w.u32()
    vertex_offset = w.o
    w.skip(vertex_count * VERTEX_STRIDE)

    n_skin = w.u32()
    if n_skin:
        fail("%s: %d skinning vertex buffers; expected none" % (name, n_skin))

    ext = w.floats(10)
    if w.o != len(data):
        fail("%s: parsed %d of %d bytes -- %d left over, so this walk is out of step with the file"
             % (name, w.o, len(data), len(data) - w.o))

    return vertex_offset, vertex_count, index_count, list(ext[4:7]), list(ext[7:10])


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


def check_tool():
    """The pin, enforced rather than commented. A tool that is PRESENT but is not the one this was
    tested against is the interesting failure: absent is easy to notice and different is not."""
    if not os.path.isfile(MESHCONVERT_EXE):
        fail("meshconvert not found at %s\n"
             "  It is a TOOL in the build, not a dependency: drop it in Tools/ or set the\n"
             "  MESHCONVERT environment variable to its full path.\n"
             "  Wanted version %s." % (MESHCONVERT_EXE, MESHCONVERT_VERSION), code=2)

    digest = hashlib.sha256()
    with open(MESHCONVERT_EXE, "rb") as fh:
        for block in iter(lambda: fh.read(65536), b""):
            digest.update(block)
    actual = digest.hexdigest()
    if actual != MESHCONVERT_SHA256:
        # A WARNING AND NOT A FAILURE, deliberately. Moving to a newer meshconvert is a thing
        # somebody may want to do on purpose, and the verification stage is what actually decides
        # whether the output is right. What this must not do is let the move happen SILENTLY.
        sys.stderr.write(
            "build_meshes: WARNING - meshconvert is not the pinned binary.\n"
            "  wanted sha256 %s (version %s)\n"
            "  found  sha256 %s\n"
            "  The conversion will run. If CheckMeshes.py is clean afterwards, update the pin in\n"
            "  this file in the same commit that changed the tool.\n"
            % (MESHCONVERT_SHA256, MESHCONVERT_VERSION, actual))


def convert(obj_dir, out_dir, names):
    check_tool()
    os.makedirs(out_dir, exist_ok=True)
    for name in names:
        obj = os.path.join(obj_dir, name + ".obj")
        if not os.path.isfile(obj):
            fail("missing %s - run meshes_to_obj.py first" % obj)
        cmd = [MESHCONVERT_EXE] + MESHCONVERT_FLAGS + ["-o", os.path.join(out_dir, name + ".cmo"), obj]
        try:
            r = subprocess.run(cmd, capture_output=True, text=True)
        except FileNotFoundError:
            fail("meshconvert not found at %s\n"
                 "  Drop it in Tools/ or set MESHCONVERT. Wanted version %s."
                 % (MESHCONVERT_EXE, MESHCONVERT_VERSION), code=2)
        if r.returncode != 0:
            fail("meshconvert failed on %s (exit %d)\n%s\n%s"
                 % (name, r.returncode, r.stdout, r.stderr))
        print("  converted %s" % name)


def inject_vertex_colours(out_dir, obj_dir, names):
    """meshconvert has no vertex-colour input from OBJ, so it writes white into the colour DWORD.
    Patch it from the .vcol sidecars, and zero the tangent while we are in the vertex.

    THE COLOUR IS PACKED R8G8B8A8, R IN THE LOW BYTE. Written little-endian, that puts R at the
    lowest address, which is what DXGI_FORMAT_R8G8B8A8_UNORM reads as .r -- so the input layout
    names the packing and nothing has to remember a swizzle. CmoReader asserts the same order and
    its unit test pins it; the two are kept in step by both saying so out loud, because a silent
    disagreement here renders every team colour as its own complement.

    THE TANGENT IS ZEROED HERE, and that is a deviation worth naming. The brief says the tangents
    are zeros by design. meshconvert does not write zeros: the CMO vertex carries a tangent, so it
    always derives one, and with the single degenerate texture coordinate this content carries the
    result is arbitrary rather than meaningful. Nothing reads the field. Zeroing it makes the
    shipped bytes match what every document says they are, and costs one store per vertex in a
    build step."""
    for name in names:
        vcol = load_vcol(os.path.join(obj_dir, name + ".vcol"))
        path = os.path.join(out_dir, name + ".cmo")
        if not os.path.isfile(path):
            fail("%s: missing -- stage 1 did not produce it" % path)

        with open(path, "rb") as fh:
            data = bytearray(fh.read())

        vertex_offset, vertex_count, index_count, _, _ = walk_cmo(data, name)

        # BEFORE WRITING ANYTHING. A count mismatch means meshconvert welded or reordered, which
        # invalidates the face-split contract the baked per-face normals depend on -- and a
        # half-patched file is worse than an unpatched one because it looks converted.
        if vertex_count != len(vcol):
            fail("%s: the CMO has %d vertices and the sidecar has %d.\n"
                 "  meshconvert welded or reordered, which breaks the face-split contract.\n"
                 "  Nothing was written." % (name, vertex_count, len(vcol)))
        if index_count != vertex_count:
            fail("%s: %d indices against %d vertices; face-split means one index per vertex"
                 % (name, index_count, vertex_count))

        for i, (r, g, b, a) in enumerate(vcol):
            base = vertex_offset + i * VERTEX_STRIDE
            struct.pack_into("<I", data, base + OFFSET_COLOUR,
                             (r & 0xFF) | ((g & 0xFF) << 8) | ((b & 0xFF) << 16) | ((a & 0xFF) << 24))
            struct.pack_into("<4f", data, base + OFFSET_TANGENT, 0.0, 0.0, 0.0, 0.0)

        with open(path, "wb") as fh:
            fh.write(data)
        print("  injected %s (%d vertices)" % (name, vertex_count))

def verify(out_dir, manifest):
    """Assert counts, extents and the landmark tests against the DECODED positions.

    Extents are asserted in WORLD UNITS with no scale factor -- the authored extent IS the object's
    size. The landmark tests are what name the axis that is wrong when the handedness is: a bounding
    box is symmetric enough on most of these hulls to survive a flip unnoticed."""
    problems = []
    by_name = {m["name"]: m for m in manifest["meshes"]}

    for m in manifest["meshes"]:
        name = m["name"]
        path = os.path.join(out_dir, name + ".cmo")
        if not os.path.isfile(path):
            problems.append("%s: missing" % name)
            continue

        with open(path, "rb") as fh:
            data = fh.read()
        vertex_offset, vertex_count, index_count, lo, hi = walk_cmo(data, name)

        if vertex_count != m["vertices"]:
            problems.append("%s: %d vertices, manifest says %d" % (name, vertex_count, m["vertices"]))
        if index_count != m["triangles"] * 3:
            problems.append("%s: %d indices, manifest says %d triangles"
                            % (name, index_count, m["triangles"]))

        positions = []
        white = 0
        nonzero_tangent = 0
        for i in range(vertex_count):
            base = vertex_offset + i * VERTEX_STRIDE
            positions.append(struct.unpack_from("<3f", data, base + OFFSET_POSITION))
            if struct.unpack_from("<I", data, base + OFFSET_COLOUR)[0] == 0xFFFFFFFF:
                white += 1
            if struct.unpack_from("<4f", data, base + OFFSET_TANGENT) != (0.0, 0.0, 0.0, 0.0):
                nonzero_tangent += 1

        # THE SIGNATURE OF SKIPPED INJECTION. Every vertex white is what meshconvert leaves behind,
        # and it renders as flat white rather than as an error -- which is a shader bug for a week.
        if white == vertex_count:
            problems.append("%s: every colour DWORD is white; the injection stage did not run" % name)
        if nonzero_tangent:
            problems.append("%s: %d tangents are non-zero" % (name, nonzero_tangent))

        for axis, label in enumerate("xyz"):
            actual_lo = min(p[axis] for p in positions)
            actual_hi = max(p[axis] for p in positions)
            for got, want, which in ((actual_lo, m["extents"]["min"][axis], "min"),
                                     (actual_hi, m["extents"]["max"][axis], "max")):
                if abs(got - want) > 0.01:
                    problems.append("%s: %s%s is %.3f, manifest says %.3f"
                                    % (name, which, label.upper(), got, want))

        # The block CMO carries itself, asserted against the manifest rather than trusted.
        for axis in range(3):
            if abs(lo[axis] - m["extents"]["min"][axis]) > 0.01 or \
               abs(hi[axis] - m["extents"]["max"][axis]) > 0.01:
                problems.append("%s: the CMO extents block disagrees with the manifest" % name)
                break

        _landmark(name, positions, problems)

    if problems:
        fail("verification failed:\n  " + "\n  ".join(problems))
    print("build_meshes: %d files verified against the manifest" % len(manifest["meshes"]))


def _landmark(name, positions, problems):
    """manifest.json's landmark tests, which name the axis that is wrong rather than saying the mesh
    looks odd. Written against the decoded positions, which is the only place a flip is visible."""
    if name == "Frigate":
        nose = max(positions, key=lambda p: p[2])
        if abs(nose[2] - 45.0) > 0.01 or abs(nose[0]) > 0.01:
            problems.append("Frigate: the max-Z vertex is %s, not the nose at x~0 z=+45 -- Z is flipped"
                            % (tuple(round(v, 2) for v in nose),))
    elif name == "ModuleShipyardL1":
        zs = [p[2] for p in positions]
        aft = [p[1] for p in positions if p[2] <= min(zs) + 4.0]
        fore = [p[1] for p in positions if p[2] >= max(zs) - 4.0]
        aft_height = max(aft) - min(aft)
        fore_height = max(fore) - min(fore)
        # Compare Y, not X width: the lattice frame is WIDER than the command block, so an X
        # comparison inverts. The command block is aft and tall; the booms are forward and flat.
        if not (aft_height > fore_height * 2.0):
            problems.append("ModuleShipyardL1: the min-Z slice is %.1f tall and the max-Z slice is "
                            "%.1f -- the tall command block should be aft, so Z is flipped"
                            % (aft_height, fore_height))
    elif name == "ModuleOreProcessorL2":
        hatch = max(positions, key=lambda p: p[1])
        if hatch[0] >= 0.0:
            problems.append("ModuleOreProcessorL2: the max-Y vertex (the hatch over the main drum) "
                            "is at x=%.2f, which is not negative -- X is mirrored" % hatch[0])
    elif name == "Station":
        size = [max(p[a] for p in positions) - min(p[a] for p in positions) for a in range(3)]
        if abs(size[0] - 220.0) > 0.01 or abs(size[1] - 54.0) > 0.01 or abs(size[2] - 220.0) > 0.01:
            problems.append("Station: size is %s, not 220 x 54 x 220 -- Y is flipped or a scale "
                            "factor crept in" % (tuple(round(v, 2) for v in size),))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--obj", default=DEFAULT_OBJ_DIR)
    ap.add_argument("--out", default=DEFAULT_OUT_DIR, help="the UWP client's Assets/Meshes directory")
    ap.add_argument("--manifest", default=DEFAULT_MANIFEST)
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
