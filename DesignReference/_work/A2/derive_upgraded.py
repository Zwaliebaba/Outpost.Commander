# Upgraded numbers (Research.cpp UpgradesFrom: percentages per rate add, one factor 1 + sum/100;
# ShipDesign.cpp ViewOf: hull hp x factor before the drive's; weapon interval / factor). Read-only.
import json, math, sys, pathlib
t = json.loads((pathlib.Path(sys.argv[1]) / "Tuning.json").read_text())
H = 100
def llround(x): return int(math.floor(x + 0.5))
def hit_h(d, a): return max(d // 4, d - a)
def reached(splash, radius):
    s = 3.0 * radius
    if splash <= 0: return 1
    k = int(splash / s)
    return sum(1 for r in range(-k, k + 1) for c in range(-k, k + 1) if (c*s)**2 + (r*s)**2 <= splash*splash)
hullpct = {"none": 0, "Hull Plating": 15, "+Composite Plating": 30, "+Ablative Armour": 35}
for label, p in hullpct.items():
    cells = []
    for h in t["hulls"]:
        for d in t["drives"]:
            hv = llround(h["hitPoints"] * H * (1 + p / 100))
            hp = llround(hv * d["hitPointsFactor"])
            cells.append(f"{h['name'][0]}+{d['name'][0]} {hp/H:g}")
    print(f"hull hp {label} (+{p}%):", ", ".join(cells))
# Fire-rate sums per weapon over all topics.
fr = {}
for r in t["research"]:
    e = r["effect"]
    if e.get("upgrade") == "weapon" and e["stat"] == "fireRate":
        fr.setdefault(e["weapon"], []).append((r["name"], e["percent"]))
for w in t["weapons"]:
    tops = fr.get(w["id"], [])
    total = sum(p for _, p in tops)
    iv = w["fireIntervalSeconds"] / (1 + total / 100)
    print(f"{w['name']}: base {w['fireIntervalSeconds']} s; topics {tops}; all {total}% -> {iv:.4f} s, {llround(iv*t['rules']['tickHz']*1000)} milliticks")
    if tops:
        first = tops[0][1]
        iv1 = w["fireIntervalSeconds"] / (1 + first / 100)
        print(f"   first topic only: {iv1:.4f} s, {llround(iv1*t['rules']['tickHz']*1000)} milliticks")
# Fully researched matrix and ratings.
best = {}
rows = {}
for w in t["weapons"]:
    total = sum(p for _, p in fr.get(w["id"], []))
    iv = w["fireIntervalSeconds"] / (1 + total / 100)
    rows[w["name"]] = []
    for h in t["hulls"]:
        f = hit_h(w["damage"]*H, h["armor"]*H) / H / iv * reached(w["splashRadiusMeters"], h["footprintRadiusMeters"])
        rows[w["name"]].append(f)
        best[h["name"]] = max(best.get(h["name"], 0), f)
for name, vals in rows.items():
    out = []
    for h, f in zip(t["hulls"], vals):
        s = f / best[h["name"]]
        out.append(f"{h['name']} {f:.2f} {s:.3f} {'Good' if s >= 2/3 else 'Fair' if s >= 1/3 else 'Poor'}")
    print("all research:", name, " | ".join(out))
g = t["structureWeapons"][0]
print("Defence gun with Autoloader:", g["fireIntervalSeconds"] / 1.2, "s")
