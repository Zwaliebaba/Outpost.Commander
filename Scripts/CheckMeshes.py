#!/usr/bin/env python3
"""
verify_cmo.py - assert the asset set against manifest.json.

Two modes. The OBJ mode is fully implemented and runnable today; run it after any regeneration and
before you spend time on conversion, because an OBJ-stage failure is a content bug and a CMO-stage
failure is a converter bug, and you want to know which one you have.

    python Scripts/verify_cmo.py --obj Assets/Meshes/obj                     # content check
    python Scripts/verify_cmo.py --cmo <ClientProject>/Assets/Meshes         # converter check

The landmark tests are the point. They are chosen so that a failure names the axis that is wrong
rather than just saying "the mesh looks odd":

    Frigate            nose is the single max-Z vertex at x ~ 0   -> a failure means Z flipped
    ModuleShipyardL1   min-Z slice is tall (command block), max-Z  -> a failure means Z flipped
                       slice is flat (lattice booms): 30 vs 5
    ModuleOreProcessorL2  max-Y vertex (hatch over the main drum) -> a failure means X mirrored
                       is at negative X
    Station            hub tower is the max-Y feature, 220x54x220 -> a failure means Y flipped
                                                                     or a scale factor crept in
"""

import argparse, json, os, struct, sys

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


TOL = 0.02


class Fail(Exception):
    pass


def read_obj(path):
    verts, norms, faces, texcoords = [], [], [], []
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("v "):
                verts.append(tuple(float(x) for x in line.split()[1:4]))
            elif line.startswith("vn "):
                norms.append(tuple(float(x) for x in line.split()[1:4]))
            elif line.startswith("f "):
                # v/vt/vn or v//vn -- the position index is the first field either way.
                faces.append([int(t.split("/")[0]) for t in line.split()[1:4]])
            elif line.startswith("vt "):
                texcoords.append(tuple(float(x) for x in line.split()[1:3]))

    # EXACTLY ONE DEGENERATE TEXTURE COORDINATE, AND IT IS NOT A UV LAYOUT.
    #
    # This check used to refuse any `vt` at all. It cannot: meshconvert always computes a tangent
    # frame when it writes CMO -- the format's vertex carries one -- and it refuses to do so without
    # texture coordinates, with no flag to suppress it. Verified against meshconvert 2026.5.8.1.
    #
    # So the contract moved rather than relaxed. One record, value (0, 0), referenced by every face
    # corner. That still catches the thing the old rule was defending against: a real UV layout
    # arriving, per-vertex, that somebody would then try to sample.
    if len(texcoords) != 1:
        raise Fail("%s has %d vt records; exactly one degenerate `vt 0 0` is required -- more than "
                   "one is a UV layout and none will not convert to CMO" % (path, len(texcoords)))
    if texcoords[0] != (0.0, 0.0):
        raise Fail("%s has vt %s; the one texture coordinate must be exactly `0 0`"
                   % (path, texcoords[0]))

    return verts, norms, faces


def read_vcol(path):
    out = []
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if line and not line.startswith("#"):
                out.append(tuple(int(x) for x in line.split()))
    return out


def bbox(verts):
    lo = [min(v[i] for v in verts) for i in range(3)]
    hi = [max(v[i] for v in verts) for i in range(3)]
    return lo, hi


def near(a, b, tol=TOL):
    return abs(a - b) <= tol


def check_obj_set(obj_dir, manifest):
    problems = []
    for m in manifest["meshes"]:
        name = m["name"]
        try:
            verts, norms, faces = read_obj(os.path.join(obj_dir, name + ".obj"))
            vcol = read_vcol(os.path.join(obj_dir, name + ".vcol"))

            if len(verts) != m["vertices"]:
                raise Fail("%d v records, manifest says %d" % (len(verts), m["vertices"]))
            if len(norms) != m["vertices"]:
                raise Fail("%d vn records, manifest says %d" % (len(norms), m["vertices"]))
            if len(faces) != m["triangles"]:
                raise Fail("%d f records, manifest says %d" % (len(faces), m["triangles"]))
            if len(vcol) != m["vertices"]:
                raise Fail("%d vcol lines, %d vertices - the sidecar must be 1:1 and in order"
                           % (len(vcol), m["vertices"]))
            if len(verts) % 3 != 0:
                raise Fail("vertex count %d is not a multiple of 3; vertices must be face-split"
                           % len(verts))

            # face-split contract: face f must reference exactly vertices 3f+1, 3f+2, 3f+3
            for i, f in enumerate(faces):
                if f != [i * 3 + 1, i * 3 + 2, i * 3 + 3]:
                    raise Fail("face %d references %s, expected %s - something re-welded or "
                               "reindexed the mesh" % (i, f, [i * 3 + 1, i * 3 + 2, i * 3 + 3]))

            # per-face normals must be constant across each triangle's three vertices
            for i in range(len(faces)):
                a, b, c = norms[i * 3], norms[i * 3 + 1], norms[i * 3 + 2]
                if a != b or b != c:
                    raise Fail("face %d has three different normals; shading must be flat, with "
                               "one baked normal per face" % i)

            for i, (r, g, b, al) in enumerate(vcol):
                if r not in (0, 255):
                    raise Fail("vertex %d R=%d; the team selector must be exactly 0 or 255" % (i, r))
                if g not in (0, 128, 255):
                    raise Fail("vertex %d G=%d; the hull tone index must be 0, 128 or 255" % (i, g))
                if b != 0:
                    raise Fail("vertex %d B=%d; B is reserved and must be 0" % (i, b))
                if al != 255:
                    raise Fail("vertex %d A=%d; A must be 255" % (i, al))

            lo, hi = bbox(verts)
            for i, axis in enumerate("XYZ"):
                if not near(lo[i], m["extents"]["min"][i]) or not near(hi[i], m["extents"]["max"][i]):
                    raise Fail("%s extent is %.2f..%.2f, manifest says %.2f..%.2f - a scale factor "
                               "or an offset crept in; the authored extent IS the object's size"
                               % (axis, lo[i], hi[i], m["extents"]["min"][i], m["extents"]["max"][i]))

            landmark(name, verts)
            print("  ok  %-24s %4d tris  %5d verts  %s"
                  % (name, m["triangles"], m["vertices"],
                     " x ".join("%g" % s for s in m["extents"]["size"])))
        except Fail as e:
            problems.append("%s: %s" % (name, e))
            print("  FAIL %s: %s" % (name, e))
        except OSError as e:
            problems.append("%s: %s" % (name, e))
            print("  FAIL %s: %s" % (name, e))
    return problems


def landmark(name, verts):
    if name == "Frigate":
        nose = max(verts, key=lambda v: v[2])
        if not near(nose[2], 45.0, 0.5):
            raise Fail("LANDMARK - max-Z vertex is at z=%.2f, expected +45. Z is flipped." % nose[2])
        if abs(nose[0]) > 0.5:
            raise Fail("LANDMARK - the nose is at x=%.2f, expected 0. X is skewed or mirrored."
                       % nose[0])
    elif name == "ModuleShipyardL1":
        # Compare the Y extent of a slice at each end, NOT the X width. X width inverts: the open
        # lattice's forward transverse frame is wider than the command block. Y separates them
        # cleanly - the command block is a tall solid mass, the lattice is a flat plane of booms.
        lo, hi = bbox(verts)
        aft = [v for v in verts if v[2] <= lo[2] + 4]
        fwd = [v for v in verts if v[2] >= hi[2] - 4]
        aft_h = max(v[1] for v in aft) - min(v[1] for v in aft)
        fwd_h = max(v[1] for v in fwd) - min(v[1] for v in fwd)
        if aft_h < fwd_h * 2:
            raise Fail("LANDMARK - the min-Z slice is %.1f units tall and the max-Z slice %.1f. "
                       "Z is flipped: the tall solid command block must be aft (min Z) and the flat "
                       "lattice forward (max Z)." % (aft_h, fwd_h))
    elif name == "ModuleOreProcessorL2":
        # The top hatch sits over the MAIN drum, which is at -X. Do not compare max|X| per side:
        # the smaller stack sits further out than the larger drum is wide, so that test inverts.
        top = max(verts, key=lambda v: v[1])
        if top[0] >= 0:
            raise Fail("LANDMARK - the max-Y vertex (the hatch over the main drum) is at x=%.2f, "
                       "expected negative. X is mirrored." % top[0])
    elif name == "Station":
        lo, hi = bbox(verts)
        size = [hi[i] - lo[i] for i in range(3)]
        for i, want in ((0, 220.0), (1, 54.0), (2, 220.0)):
            if not near(size[i], want, 0.5):
                raise Fail("LANDMARK - %s size is %.2f, expected %g. A scale factor crept in."
                           % ("XYZ"[i], size[i], want))
        top = max(verts, key=lambda v: v[1])
        if (top[0] ** 2 + top[2] ** 2) ** 0.5 > 46:
            raise Fail("LANDMARK - the max-Y feature is %.1f units off centre; the hub tower is "
                       "central, so Y is flipped." % (top[0] ** 2 + top[2] ** 2) ** 0.5)


# --- THE CMO WALK -------------------------------------------------------------------------------
# The layout is BuildMeshes.py's, which grounded it field by field against real converter output.
# It is duplicated here rather than imported because these two scripts are run at different moments
# for different reasons -- the build produces, this verifies -- and a shared module would make the
# verification depend on the thing it verifies.
#
# NeuronClient/CmoReader is the third statement of it, and the three are kept honest by all of them
# asserting the same counts and extents out of manifest.json rather than by reading each other.
#
# The field that does not survive memory: a material's ambient, diffuse, specular and emissive are
# float4, not float3, with the specular power a single float between specular and emissive.

VERTEX_STRIDE = 52
OFFSET_POSITION = 0
OFFSET_COLOUR = 40


def _u32(d, o):
    return struct.unpack_from("<I", d, o)[0], o + 4


def _skip_wstr(d, o):
    n, o = _u32(d, o)
    return o + (n * 2)


def decode_cmo(data, name):
    """(positions, colours, triangle_count, extents_min, extents_max), or raises Fail."""
    o = 0
    n_meshes, o = _u32(data, o)
    if n_meshes != 1:
        raise Fail("%s: %d meshes in one file; the handoff is one submesh per file" % (name, n_meshes))
    o = _skip_wstr(data, o)

    n_materials, o = _u32(data, o)
    for _ in range(n_materials):
        o = _skip_wstr(data, o)
        o += 4 * (4 + 4 + 4 + 1 + 4)
        o += 4 * 16
        o = _skip_wstr(data, o)
        for _ in range(8):
            o = _skip_wstr(data, o)

    skeletal = data[o]
    o += 1

    n_submeshes, o = _u32(data, o)
    o += n_submeshes * 20

    index_count = 0
    n_ib, o = _u32(data, o)
    for _ in range(n_ib):
        n, o = _u32(data, o)
        index_count += n
        o += 2 * n

    n_vb, o = _u32(data, o)
    if n_vb != 1:
        raise Fail("%s: %d vertex buffers; expected exactly one" % (name, n_vb))
    vertex_count, o = _u32(data, o)

    positions = []
    colours = []
    for v in range(vertex_count):
        base = o + (v * VERTEX_STRIDE)
        positions.append(struct.unpack_from("<3f", data, base + OFFSET_POSITION))
        colours.append(struct.unpack_from("<I", data, base + OFFSET_COLOUR)[0])
    o += vertex_count * VERTEX_STRIDE

    n_skin, o = _u32(data, o)
    for _ in range(n_skin):
        n, o = _u32(data, o)
        o += n * 32

    ext = struct.unpack_from("<10f", data, o)
    o += 40

    if skeletal:
        raise Fail("%s: the skeletal-animation byte is set; nothing in this handoff is skinned" % name)

    if o != len(data):
        raise Fail("%s: parsed %d of %d bytes -- this walk is out of step with the file"
                   % (name, o, len(data)))

    return positions, colours, index_count // 3, list(ext[4:7]), list(ext[7:10])


def check_cmo_set(cmo_dir, manifest):
    """CMO-stage verification: every file decodes, and what comes out of it is what the manifest
    says went in."""
    missing = [m["name"] for m in manifest["meshes"]
               if not os.path.isfile(os.path.join(cmo_dir, m["name"] + ".cmo"))]
    if missing:
        return ["missing .cmo: " + ", ".join(missing)]

    problems = []
    for m in manifest["meshes"]:
        name = m["name"]
        with open(os.path.join(cmo_dir, name + ".cmo"), "rb") as fh:
            data = fh.read()
        try:
            positions, colours, triangles, lo, hi = decode_cmo(data, name)
        except Fail as e:
            problems.append(str(e))
            continue

        if len(positions) != m["vertices"]:
            problems.append("%s: %d vertices in the file, %d in the manifest"
                            % (name, len(positions), m["vertices"]))
        if triangles != m["triangles"]:
            problems.append("%s: %d triangles in the file, %d in the manifest"
                            % (name, triangles, m["triangles"]))

        want_lo = m["extents"]["min"]
        want_hi = m["extents"]["max"]
        for i in range(3):
            if not near(lo[i], want_lo[i]) or not near(hi[i], want_hi[i]):
                problems.append("%s: %s extent is [%.2f, %.2f], the manifest says [%g, %g]"
                                % (name, "XYZ"[i], lo[i], hi[i], want_lo[i], want_hi[i]))

        # ALL WHITE OR ALL ZERO IS THE SIGNATURE OF SKIPPED VERTEX-COLOUR INJECTION, and it is a
        # failure that renders: the material's diffuse is white on purpose because the albedo
        # arrives entirely through this channel, so a hull with no colour draws white rather than
        # not drawing at all.
        distinct = set(colours)
        if distinct <= {0xFFFFFFFF} or distinct <= {0}:
            problems.append("%s: every vertex colour is %s -- the vcol injection was skipped"
                            % (name, "white" if 0xFFFFFFFF in distinct else "zero"))

        # The landmark tests again, on the DECODED positions rather than on the OBJ. An OBJ-stage
        # pass and a CMO-stage failure is a converter bug, which is the whole reason the two stages
        # are separate.
        try:
            landmark(name, positions)
        except Fail as e:
            problems.append("%s: %s" % (name, e))

    problems += check_catalog_sizes(manifest)
    problems += check_generated_catalog()

    print("  %d .cmo files decoded" % len(manifest["meshes"]))
    return problems


def check_generated_catalog():
    """GameClient/MeshCatalog.g.h is generated from this same manifest, so a manifest that moved
    without the header moving is a client asserting against figures that are no longer true."""
    import subprocess
    script = os.path.join(REPO_ROOT, "Scripts", "BuildMeshCatalog.py")
    result = subprocess.run([sys.executable, script, "--check"], capture_output=True, text=True)
    if result.returncode != 0:
        return [result.stderr.strip() or "MeshCatalog.g.h is stale"]
    print("  MeshCatalog.g.h is current")
    return []


# --- Q37: TWO STATEMENTS OF ONE FIGURE ----------------------------------------------------------
# ADR-005 never scales a mesh at draw time, so a hull's authored extent IS its size -- and R24 wants
# that size NAMED in the catalog rather than discovered by loading geometry. Both statements exist,
# so this is the script between them.
#
# The catalog states a BOUND and rounds up: it is what spaces things so they do not overlap and how
# far in front of a station a new ship appears, and both want the larger number.

CATALOG_PATH = os.path.join(REPO_ROOT, "GameCore", "Catalog.cpp")

# Which meshes draw which hull. The Cruiser is deliberately absent: it is cut from the MVP
# (GameDesign.md section 6) so nothing authored a mesh for it, and M4 authors to the catalog's 150
# rather than the other way round.
#
# A HULL IS BOUNDED BY EVERY MESH THAT DRAWS IT, and since M2.10b the ModuleFrame is drawn by five: the
# bare frame and one mesh per module level. Its size is the longest of them, rounded up.
HULL_MESHES = {
    "Scout": ["Scout"],
    "Frigate": ["Frigate"],
    "Station": ["Station"],
    "ModuleFrame": ["ModuleFrame", "ModuleShipyardL1", "ModuleShipyardL2", "ModuleOreProcessorL1", "ModuleOreProcessorL2"],
}


def check_catalog_sizes(manifest):
    import re
    problems = []
    try:
        with open(CATALOG_PATH, "r", encoding="utf-8") as fh:
            catalog = fh.read()
    except OSError as e:
        return ["cannot read %s: %s" % (CATALOG_PATH, e)]

    by_name = {m["name"]: m for m in manifest["meshes"]}
    for hull, meshes in sorted(HULL_MESHES.items()):
        entries = [by_name.get(mesh) for mesh in meshes]
        missing = [mesh for mesh, entry in zip(meshes, entries) if entry is None]
        if missing:
            problems.append("Q37: the manifest has no mesh named %s for hull %s" % (", ".join(missing), hull))
            continue
        longest_mesh, longest = max(((mesh, entry["extents"]["longest"]) for mesh, entry in zip(meshes, entries)),
                                    key=lambda pair: pair[1])

        # The row, then its sizeUnits. Matched on the hull identity so a reordered table still works.
        row = re.search(r"\{\.id = HullId::" + hull + r"\b(.*?)\}", catalog, re.S)
        if row is None:
            problems.append("Q37: no HullId::%s row in Catalog.cpp" % hull)
            continue

        stated = re.search(r"\.sizeUnits = (\d+)", row.group(1))
        if stated is None:
            problems.append("Q37: HullId::%s states no sizeUnits" % hull)
            continue

        import math
        authored = math.ceil(longest - 1e-9)
        if int(stated.group(1)) != authored:
            problems.append("Q37: HullId::%s states sizeUnits %s, %s.cmo is %g units along its "
                            "longest axis (ceiling %d)"
                            % (hull, stated.group(1), longest_mesh, longest, authored))

    print("  Q37: %d hull sizes checked against their meshes" % len(HULL_MESHES))
    return problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--obj", nargs="?", const=DEFAULT_OBJ_DIR)
    ap.add_argument("--cmo", nargs="?", const=DEFAULT_OUT_DIR)
    ap.add_argument("--manifest", default=DEFAULT_MANIFEST)
    args = ap.parse_args()
    if not args.obj and not args.cmo:
        ap.error("pass --obj or --cmo")

    with open(args.manifest, "r", encoding="utf-8") as fh:
        manifest = json.load(fh)

    problems = []
    if args.obj:
        print("verify_cmo: OBJ content check, %d meshes" % len(manifest["meshes"]))
        problems += check_obj_set(args.obj, manifest)
    if args.cmo:
        print("verify_cmo: CMO converter check")
        problems += check_cmo_set(args.cmo, manifest)

    if problems:
        sys.stderr.write("\nverify_cmo: %d problem(s)\n  %s\n" % (len(problems), "\n  ".join(problems)))
        sys.exit(1)
    print("verify_cmo: all checks passed")


if __name__ == "__main__":
    main()
