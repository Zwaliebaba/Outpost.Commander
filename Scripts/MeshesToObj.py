#!/usr/bin/env python3
"""
meshes_to_obj.py — regenerate the OBJ/MTL/VCOL set from the design handoff's meshes.json.

The integration package already ships the generated output in integration/obj/, so you only need
this when meshes.json changes. It is a pure transform: no welding, no reordering, no reindexing.

    python Scripts/meshes_to_obj.py --input design_handoff_meshes/meshes.json --out Assets/Meshes/obj

Why OBJ and not FBX: meshconvert eats FBX, OBJ, VBO and SDKMESH. meshes.json is none of them, so
something has to bridge, and the bridge is cheaper and reviewable if it targets OBJ. OBJ has no
vertex-colour field in the base spec, so the R/G selector bytes travel out-of-band in a .vcol
sidecar. Do NOT switch to the non-standard 6-float `v x y z r g b` extension: meshconvert ignores
it silently, which is the worst possible failure for a channel that carries team identity.

THE SINGLE `vt 0 0` IS REQUIRED AND IS NOT A UV LAYOUT. The handoff's contract said to emit no
texture coordinate at all. meshconvert cannot write CMO from such a file -- the CMO vertex carries
a tangent, so the tool always computes a tangent frame, and it refuses without texture
coordinates:

    ERROR: Computing tangents/bi-tangents requires texture coordinates

There is no flag that turns it off; -t and -tb ADD a tangent frame and their absence does not
remove the requirement. Verified against meshconvert 2026.5.8.1. One degenerate coordinate,
referenced by every face corner, satisfies it and changes no geometry: the converted Scout came
back with its 474 vertices and 158 faces intact, its index buffer still sequential, its extents
matching the manifest, and its CMO texcoords all zero.
"""

import argparse, json, os, sys

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
DEFAULT_MESHES_JSON = os.path.join(REPO_ROOT, "Design", "design_handoff_meshes", "Design", "meshes.json")


MTL = """# Outpost Commander - one material, identical on every mesh.
# Diffuse is white on purpose: all albedo arrives through the vertex colour channel.
newmtl OC_Hull
Ka 0.200 0.250 0.320
Kd 1.000 1.000 1.000
Ks 0.220 0.240 0.280
Ns 18.0
Ke 0.000 0.000 0.000
d 1.0
illum 2
"""


def fail(msg):
    sys.stderr.write("meshes_to_obj: %s\n" % msg)
    sys.exit(1)


def check(name, mesh):
    """The handoff guarantees all of this. A failure here means the content changed,
    not that the exporter is wrong - so it is fatal, not a warning."""
    nv = mesh["vertices"]
    if nv % 3 != 0:
        fail("%s: vertex count %d is not a multiple of 3 (vertices must be face-split)" % (name, nv))
    if len(mesh["positions"]) != nv * 3:
        fail("%s: positions length %d != vertices*3" % (name, len(mesh["positions"])))
    if len(mesh["normals"]) != nv * 3:
        fail("%s: normals length %d != vertices*3" % (name, len(mesh["normals"])))
    if len(mesh["colors"]) != nv * 4:
        fail("%s: colors length %d != vertices*4" % (name, len(mesh["colors"])))
    if len(mesh["indices"]) != mesh["triangles"] * 3:
        fail("%s: indices length %d != triangles*3" % (name, len(mesh["indices"])))
    if nv > 65535:
        fail("%s: %d vertices exceeds the uint16 index limit" % (name, nv))
    c = mesh["colors"]
    for i in range(0, len(c), 4):
        if c[i] not in (0, 255):
            fail("%s: vertex %d has R=%d; the team selector must be exactly 0 or 255" % (name, i // 4, c[i]))
        if c[i + 1] not in (0, 128, 255):
            fail("%s: vertex %d has G=%d; the hull tone index must be 0, 128 or 255" % (name, i // 4, c[i + 1]))
        if c[i + 2] != 0:
            fail("%s: vertex %d has B=%d; B is reserved and must be 0" % (name, i // 4, c[i + 2]))
        if c[i + 3] != 255:
            fail("%s: vertex %d has A=%d; A must be 255" % (name, i // 4, c[i + 3]))
    # indices are trivially sequential because the vertices are already face-split
    for i, v in enumerate(mesh["indices"]):
        if v != i:
            fail("%s: index %d is %d, expected %d. The arrays are face-split; "
                 "reindexed input means something re-welded the mesh upstream." % (name, i, v, i))


def num(v):
    """Compact, round-trip-safe float formatting. Avoids 1.0000000000000002 noise in diffs."""
    if v == int(v):
        return str(int(v))
    return repr(round(v, 6))


# ENCODING IS NOT OPTIONAL ON WINDOWS, and the handoff's script did not say it. `open(..., "w")`
# takes the locale default -- cp1252 here -- and the per-mesh notes carried out of meshes.json have
# em-dashes in them. The delivered files were UTF-8, so regenerating on this machine silently
# rewrote thirteen files in a second encoding; the verifier's own utf-8 read is what caught it, one
# step later, as a decode error on a file it had just been told was fine. Every write below says
# utf-8.
def write_mesh(out_dir, name, mesh, doc_format):
    nv = mesh["vertices"]
    p, n, c = mesh["positions"], mesh["normals"], mesh["colors"]
    ext = mesh["extents"]

    lines = [
        "# Outpost Commander - %s" % name,
        "# %s" % mesh.get("note", ""),
        "# Generated from meshes.json (%s) - do not hand-edit." % doc_format,
        "# LEFT-HANDED, Y up, +Z forward. Plane is Y=0. Mesh origin = simulated entity position.",
        "# World units 1:1 - the authored extent IS the object's size; never scaled at draw time.",
        "# Face-split vertices, baked per-face normals. No welding, no smoothing groups.",
        "# ONE DEGENERATE TEXTURE COORDINATE, and it is not a UV layout. meshconvert always computes",
        "# a tangent frame when it writes CMO -- the format's vertex carries one -- and it refuses to",
        "# do so without texture coordinates. There is no flag to suppress it. So every face corner",
        "# references this single vt 0 0. The CMO's texcoord field comes back zero, which is what the",
        "# brief asks for; nothing reads it and nothing should start.",
        "# extents  min %s  max %s  size %s" % (
            " ".join(num(x) for x in ext["min"]),
            " ".join(num(x) for x in ext["max"]),
            " ".join(num(x) for x in ext["size"])),
        "# %d triangles, %d vertices" % (mesh["triangles"], nv),
        "# Vertex colour selector is NOT in this file. See %s.vcol - one line per v record, same order." % name,
        "mtllib OC_Hull.mtl",
        "o %s" % name,
    ]
    lines += ["v %s %s %s" % (num(p[i * 3]), num(p[i * 3 + 1]), num(p[i * 3 + 2])) for i in range(nv)]
    lines += ["vn %s %s %s" % (num(n[i * 3]), num(n[i * 3 + 1]), num(n[i * 3 + 2])) for i in range(nv)]
    # THE ONE TEXTURE COORDINATE. One record, not one per vertex: a per-vertex UV would be a layout,
    # and a layout is a thing somebody would later try to use.
    lines.append("vt 0 0")
    lines += ["usemtl OC_Hull", "s off"]
    for f in range(mesh["triangles"]):
        a = f * 3 + 1
        # v/vt/vn, with the vt index always 1 -- there is only ever one to point at.
        lines.append("f %d/1/%d %d/1/%d %d/1/%d" % (a, a, a + 1, a + 1, a + 2, a + 2))
    with open(os.path.join(out_dir, name + ".obj"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")

    vlines = [
        '# %s - vertex colour selector, one "R G B A" line per v record in %s.obj, same order.' % (name, name),
        "# R: 0 = hull palette, 255 = the owning player's TEAM colour. No other value is emitted.",
        "# G: 0 = HULL.DEEP, 128 = HULL.BASE, 255 = HULL.EDGE. Ignored where R = 255.",
        "# B: reserved, always 0.   A: always 255.",
        "# %d vertices." % nv,
    ]
    vlines += ["%d %d %d %d" % (c[i * 4], c[i * 4 + 1], c[i * 4 + 2], c[i * 4 + 3]) for i in range(nv)]
    with open(os.path.join(out_dir, name + ".vcol"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(vlines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", default=DEFAULT_MESHES_JSON)
    ap.add_argument("--out", default=DEFAULT_OBJ_DIR)
    args = ap.parse_args()

    with open(args.input, "r", encoding="utf-8") as fh:
        doc = json.load(fh)
    if not str(doc.get("format", "")).startswith("outpost-commander-mesh-handoff/"):
        fail("%s is not a mesh handoff document (format=%r)" % (args.input, doc.get("format")))

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "OC_Hull.mtl"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(MTL)

    tris = verts = 0
    for name, mesh in doc["meshes"].items():
        check(name, mesh)
        write_mesh(args.out, name, mesh, doc["format"])
        tris += mesh["triangles"]
        verts += mesh["vertices"]
        print("  %-24s %4d tris  %5d verts" % (name, mesh["triangles"], mesh["vertices"]))

    print("meshes_to_obj: %d meshes, %d triangles, %d vertices -> %s"
          % (len(doc["meshes"]), tris, verts, args.out))


if __name__ == "__main__":
    main()
