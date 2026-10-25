# Every design's damage cards as the designer writes them (Hud.cpp DescribeDesigner): per ship "{:.1f}",
# "x5 ships" when a splash reaches more than one, "{:.1f} / 100 ore" = per ship x 100 / design cost, and the
# rating against the best any design does to that hull (BestOfAll). Base research. Read-only; prints markdown rows.
import json, math, sys, pathlib
t = json.loads((pathlib.Path(sys.argv[1]) / "Tuning.json").read_text())
H = 100
def hit_h(d, a): return max(d // 4, d - a)
def reached(splash, radius):
    s = 3.0 * radius
    if splash <= 0: return 1
    k = int(splash / s)
    return sum(1 for r in range(-k, k + 1) for c in range(-k, k + 1) if (c*s)**2 + (r*s)**2 <= splash*splash)
def f(w, h): return hit_h(w["damage"]*H, h["armor"]*H) / H / w["fireIntervalSeconds"] * reached(w["splashRadiusMeters"], h["footprintRadiusMeters"])
best = {h["id"]: max(f(w, h) for w in t["weapons"]) for h in t["hulls"]}
def rate(x, b):
    s = x / b
    return "Good" if s >= 2/3 else "Fair" if s >= 1/3 else "Poor"
names = {(s["hull"], s["drive"], s["weapon"]): s["name"] for s in t["startingDesigns"]}
print("| Design | Cost (Ore) | vs Small: per 100 Ore | vs Medium: per 100 Ore | vs Large: per 100 Ore |")
print("|---|---|---|---|---|")
for h in t["hulls"]:
    for d in t["drives"]:
        for w in t["weapons"]:
            cost = h["cost"] + d["cost"] + w["cost"]
            nm = f"{h['name']}+{d['name']}+{w['name']}"
            if (h["id"], d["id"], w["id"]) in names:
                nm += f" ({names[(h['id'], d['id'], w['id'])]})"
            cells = [f"{f(w, hh)*100/cost:.1f}" for hh in t["hulls"]]
            print(f"| {nm} | {cost} | " + " | ".join(cells) + " |")
