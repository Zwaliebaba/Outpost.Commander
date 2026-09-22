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

import argparse, json, os, sys

TOL = 0.02


class Fail(Exception):
    pass


def read_obj(path):
    verts, norms, faces = [], [], []
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("v "):
                verts.append(tuple(float(x) for x in line.split()[1:4]))
            elif line.startswith("vn "):
                norms.append(tuple(float(x) for x in line.split()[1:4]))
            elif line.startswith("f "):
                faces.append([int(t.split("//")[0]) for t in line.split()[1:4]])
            elif line.startswith("vt "):
                raise Fail("%s emits vt records; there is no UV layout and nothing reads one" % path)
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


def check_cmo_set(cmo_dir, manifest):
    """CMO-stage verification. Read each file through the same CmoReader the client uses - not a
    second parser written for the test, or you are verifying two bugs against each other."""
    missing = [m["name"] for m in manifest["meshes"]
               if not os.path.isfile(os.path.join(cmo_dir, m["name"] + ".cmo"))]
    if missing:
        return ["missing .cmo: " + ", ".join(missing)]
    # TODO(implementer): for each file assert vertex count, triangle count and bounding box against
    # the manifest, then run the same landmark() checks on the decoded positions. Also assert the
    # colour DWORDs are not all white/zero - that is the signature of skipped vcol injection.
    print("  %d .cmo files present; decode assertions not implemented yet" % len(manifest["meshes"]))
    return []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--obj")
    ap.add_argument("--cmo")
    ap.add_argument("--manifest", default="design_handoff_meshes/integration/manifest.json")
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
