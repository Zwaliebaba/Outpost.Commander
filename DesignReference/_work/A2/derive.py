# Derives A2's tables from Tuning.json and Matchups.json with the game's own rules (read-only):
#   ShipDesign.cpp ViewOf / DesignStats.h DesignStatsOf, HitHundredths, DamagePerSecond, ShipsReached,
#   FormationDamagePerSecond; Hud.cpp BestOfAll, RatingOf (GOOD_SHARE 2/3, FAIR_SHARE 1/3);
#   Simulation.cpp StructureHitPointsAt.
import json, math, sys, pathlib
assets = pathlib.Path(sys.argv[1])
t = json.loads((assets / "Tuning.json").read_text())
m = json.loads((assets / "Matchups.json").read_text())
H = 100
hulls = {h["id"]: h for h in t["hulls"]}
drives = {d["id"]: d for d in t["drives"]}
weapons = {w["id"]: w for w in t["weapons"]}
modules = {x["id"]: x for x in t["modules"]}

def llround(x):
    return int(math.floor(x + 0.5)) if x >= 0 else -int(math.floor(-x + 0.5))

def hit_h(dmg_h, arm_h):
    return max(dmg_h // 4, dmg_h - arm_h)

def ships_reached(splash, radius):
    spacing = 3.0 * radius
    if splash <= 0 or spacing <= 0:
        return 1
    steps = int(splash / spacing)
    n = 0
    for r in range(-steps, steps + 1):
        for c in range(-steps, steps + 1):
            if (c * spacing) ** 2 + (r * spacing) ** 2 <= splash * splash:
                n += 1
    return n

def dps(w, arm):
    return hit_h(w["damage"] * H, arm * H) / H / w["fireIntervalSeconds"]

def fdps(w, hull):
    return dps(w, hull["armor"]) * ships_reached(w["splashRadiusMeters"], hull["footprintRadiusMeters"])

print("== hull x drive ==")
for h in hulls.values():
    for d in drives.values():
        hp = llround(h["hitPoints"] * H * d["hitPointsFactor"])
        spd = h["speedMetersPerSecond"] * d["speedFactor"]
        turn = h["turnRateDegreesPerSecond"] * d["turnRateFactor"]
        radius = spd / math.radians(turn)
        print(f"{h['name']}+{d['name']}: hp {hp/H:.2f} armor {h['armor']} speed {spd:.2f} (+SA {spd*0.9:.2f}) turn {turn:.2f} deg/s "
              f"radius {radius:.1f} m cost {h['cost']+d['cost']} build {h['buildSeconds']} cp {h['commandPoints']}")

print("== fire interval ticks ==")
for w in weapons.values():
    print(w["name"], w["fireIntervalSeconds"] * t["rules"]["tickHz"], "ticks; sight", w["rangeMeters"] + t["sight"]["weaponMarginMeters"])

print("== damage matrix (base research) ==")
best = {}
for hid, h in hulls.items():
    best[hid] = max(fdps(w, h) for w in weapons.values())
for w in weapons.values():
    row = []
    for hid, h in hulls.items():
        hit = hit_h(w["damage"] * H, h["armor"] * H) / H
        reach = ships_reached(w["splashRadiusMeters"], h["footprintRadiusMeters"])
        f = fdps(w, h)
        share = f / best[hid]
        rating = "Good" if share >= 2/3 else "Fair" if share >= 1/3 else "Poor"
        row.append(f"{h['name']}: hit {hit:g} dps {dps(w, h['armor']):.4f} x{reach} = {f:.4f} share {share:.3f} {rating}")
    print(w["name"], " | ".join(row))
print("best per hull", {hulls[k]["name"]: round(v, 4) for k, v in best.items()})

print("== other targets (hit after armor, dps) ==")
targets = [("Constructor", t["constructor"]["armor"])] + [(s["name"], s["armor"]) for s in t["structures"]]
for w in weapons.values():
    print(w["name"], ", ".join(f"{n} {hit_h(w['damage']*H, a*H)/H:g} ({dps(w, a):.2f}/s)" for n, a in targets))
g = t["structureWeapons"][0]
print(g["name"], ", ".join(f"{h['name']} {hit_h(g['damage']*H, h['armor']*H)/H:g} ({hit_h(g['damage']*H, h['armor']*H)/H/g['fireIntervalSeconds']:.2f}/s)" for h in hulls.values()),
      f"Constructor {hit_h(g['damage']*H, t['constructor']['armor']*H)/H:g}")

print("== starting designs ==")
for s in t["startingDesigns"]:
    h, d, w = hulls[s["hull"]], drives[s["drive"]], weapons[s["weapon"]]
    cost = h["cost"] + d["cost"] + w["cost"]
    cells = []
    for hh in hulls.values():
        f = fdps(w, hh)
        cells.append(f"{hh['name']} {f:.2f}/ship {f*100/cost:.2f}/100ore")
    print(s["name"], cost, "Ore;", "; ".join(cells), f"(+SA {cost+120}: " + ", ".join(f"{fdps(w,hh)*100/(cost+120):.2f}" for hh in hulls.values()) + ")")

print("== best bars ==")
allstats = []
for h in hulls.values():
    for d in drives.values():
        for w in weapons.values():
            allstats.append((llround(h["hitPoints"]*H*d["hitPointsFactor"]), h["armor"], h["speedMetersPerSecond"]*d["speedFactor"], w["rangeMeters"], h["cost"]+d["cost"]+w["cost"], h["buildSeconds"]))
print("hp", max(a[0] for a in allstats)/H, "armor", max(a[1] for a in allstats), "speed", max(a[2] for a in allstats), "range", max(a[3] for a in allstats),
      "cost", max(a[4] for a in allstats) + max(x["cost"] for x in modules.values()), "build", max(a[5] for a in allstats))

print("== structure hp per level (no research / with Reinforced Structures +25%) ==")
pct = t["rules"]["levelHitPointsPercent"]
for s in t["structures"]:
    top = 1 + len(s.get("levels", []))
    print(s["name"], [ (llround(s["hitPoints"]*H*(1.0 + pct*(L-1)/100))/H, llround(s["hitPoints"]*H*(1.25 + pct*(L-1)/100))/H) for L in range(1, top+1)])

print("== research totals ==")
for tier in (1, 2, 3):
    topics = [r for r in t["research"] if r["tier"] == tier]
    print("tier", tier, len(topics), "topics", sum(r["researchSeconds"] for r in topics), "s", sum(r["cost"] for r in topics), "Ore")
print("all", len(t["research"]), sum(r["researchSeconds"] for r in t["research"]), "s", sum(r["cost"] for r in t["research"]), "Ore")

print("== matchups ==")
for mu in m["matchups"]:
    sides = []
    for side in mu["sides"]:
        ore = sum((hulls[g["hull"]]["cost"]+drives[g["drive"]]["cost"]+weapons[g["weapon"]]["cost"]+(modules[g["module"]]["cost"] if "module" in g else 0))*g["count"] for g in side)
        cp = sum(hulls[g["hull"]]["commandPoints"]*g["count"] for g in side)
        hp = sum(llround(hulls[g["hull"]]["hitPoints"]*H*drives[g["drive"]]["hitPointsFactor"])*g["count"] for g in side)/H
        sides.append(f"{ore} Ore, {cp} cp, {hp:g} hp")
    print(mu["name"], mu["distanceMeters"], " vs ".join(sides))
