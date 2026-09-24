#!/usr/bin/env python3
"""
meshes_to_obj.py — regenerate the OBJ/MTL/VCOL set from the design handoff's meshes.json.

The integration package already ships the generated output in integration/obj/, so you only need
this when meshes.json changes. It is a pure transform: no welding, no reordering, no reindexing.

    python Scripts/meshes_to_obj.py            # Design/meshes.json -> Assets/Meshes/obj

Why OBJ and not FBX: meshconvert eats FBX, OBJ, VBO and SDKMESH. meshes.json is none of them, so
something has to bridge, and the bridge is cheaper and reviewable if it targets OBJ. OBJ has no
vertex-colour field in the base spec, so the R/G selector bytes travel out-of-band in a .vcol
sidecar. Do NOT switch to the non-standard 6-float `v x y z r g b` extension: meshconvert ignores
it silently, which is the worst possible failure for a channel that carries team identity.
"""

import argparse, json, os, sys

PKG_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

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
        "# Face-split vertices, baked per-face normals. No welding, no smoothing groups, no vt.",
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
    lines += ["usemtl OC_Hull", "s off"]
    for f in range(mesh["triangles"]):
        a = f * 3 + 1
        lines.append("f %d//%d %d//%d %d//%d" % (a, a, a + 1, a + 1, a + 2, a + 2))
    with open(os.path.join(out_dir, name + ".obj"), "w", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")

    vlines = [
        '# %s - vertex colour selector, one "R G B A" line per v record in %s.obj, same order.' % (name, name),
        "# R: 0 = hull palette, 255 = the owning player's TEAM colour. No other value is emitted.",
        "# G: 0 = HULL.DEEP, 128 = HULL.BASE, 255 = HULL.EDGE. Ignored where R = 255.",
        "# B: reserved, always 0.   A: always 255.",
        "# %d vertices." % nv,
    ]
    vlines += ["%d %d %d %d" % (c[i * 4], c[i * 4 + 1], c[i * 4 + 2], c[i * 4 + 3]) for i in range(nv)]
    with open(os.path.join(out_dir, name + ".vcol"), "w", newline="\n") as fh:
        fh.write("\n".join(vlines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", default=os.path.join(PKG_ROOT, "Design", "meshes.json"))
    ap.add_argument("--out", default=os.path.join(PKG_ROOT, "Assets", "Meshes", "obj"))
    args = ap.parse_args()

    with open(args.input, "r", encoding="utf-8") as fh:
        doc = json.load(fh)
    if not str(doc.get("format", "")).startswith("outpost-commander-mesh-handoff/"):
        fail("%s is not a mesh handoff document (format=%r)" % (args.input, doc.get("format")))

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "OC_Hull.mtl"), "w", newline="\n") as fh:
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
