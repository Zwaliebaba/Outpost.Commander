# Counts hp_<tag> empties in each glTF source (ADR-018 decision 4). Read-only.
import json, struct, sys, pathlib, re, collections
root = pathlib.Path(sys.argv[1])
for path in sorted(root.glob("*/*.glb")):
    data = path.read_bytes()
    magic, version, length = struct.unpack_from("<III", data, 0)
    chunk_len, chunk_type = struct.unpack_from("<II", data, 12)
    doc = json.loads(data[20:20 + chunk_len])
    counts = collections.Counter()
    for node in doc.get("nodes", []):
        name = node.get("name", "")
        m = re.match(r"hp_([a-z0-9]+)(\.\d+)?$", name)
        if m:
            counts[m.group(1)] += 1
    print(f"{path.parent.name}/{path.name}: " + ", ".join(f"{k}={v}" for k, v in sorted(counts.items())))
