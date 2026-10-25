# V2: an independent port of Neuron::Random (NeuronServer/Random.cpp) and Outpost::PlaceContent (GameLogic/Map.cpp:354-508),
# float32 where the C++ uses float, to replay a seed's map content. Checked against RandomTests' pinned draws first.
# Usage: python3 replay.py [seed]
import json, math, sys
import numpy as np
F = np.float32
M64 = (1 << 64) - 1

def rotl(x, k):
    return ((x << k) | (x >> (64 - k))) & M64

class Random:
    def __init__(self, seed):
        s = seed & M64
        self.st = []
        for _ in range(4):
            s = (s + 0x9E3779B97F4A7C15) & M64
            z = s
            z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
            z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
            self.st.append(z ^ (z >> 31))
    def u64(self):
        s = self.st
        result = (rotl((s[1] * 5) & M64, 7) * 9) & M64
        t = (s[1] << 17) & M64
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t; s[3] = rotl(s[3], 45)
        return result
    def below(self, bound):
        product = (self.u64() >> 32) * bound
        low = product & 0xFFFFFFFF
        if low < bound:
            threshold = ((1 << 32) - bound) % bound
            while low < threshold:
                product = (self.u64() >> 32) * bound
                low = product & 0xFFFFFFFF
        return product >> 32
    def unit(self):
        return (self.u64() >> 11) * 2.0 ** -53

def selftest():
    z = Random(0)
    assert [z.u64() for _ in range(4)] == [0x99EC5F36CB75F2B4, 0xBF6E1F784956452A, 0x1A5F849D4933E6E0, 0x6AA594F1262D2D2C]
    o = Random(0xDEADBEEF)
    assert [o.u64() for _ in range(2)] == [0xC5555444A74D7E83, 0x65C30D37B4B16E38]
    r = Random(42)
    assert [r.below(6) for _ in range(10)] == [0, 2, 4, 5, 5, 4, 4, 5, 4, 3]
    r = Random(42)
    assert r.unit() == 0.08386297105988216 and r.unit() == 0.3789802506626686

def dist(a, b):
    dx = F(a[0] - b[0]); dz = F(a[1] - b[1])
    return F(np.sqrt(F(F(dx * dx) + F(dz * dz))))

def place(seed, mapfile, tuningfile):
    m = json.load(open(mapfile)); tun = json.load(open(tuningfile))
    topics = [t['id'] for t in tun['research']]
    half = m['sizeMeters'] / 2.0
    gap = F(m['minimumGapMeters'])
    P = m['placement']; border = F(P['borderMeters']); nodeClear = F(P['nodeClearanceMeters']); spacing = F(P['oreSpacingMeters'])
    sectors = m['sectors']
    kinds = {k['name']: k for k in m['sectorKinds']}
    ores = [dict(pos=(F(a['xMeters']), F(a['zMeters'])), r=F(a['radiusMeters']), yld=a['yield'], seeded=False, sector=None) for a in m['oreAsteroids']]
    fields = [((F(a['xMeters']), F(a['zMeters'])), F(a['radiusMeters'])) for a in m['asteroidFields']]
    starts = [(F(s['xMeters']), F(s['zMeters'])) for s in m['starts']]
    def contains(s, p):
        return s['minXMeters'] <= p[0] <= s['maxXMeters'] and s['minZMeters'] <= p[1] <= s['maxZMeters']
    def mirror(s):
        across = (F(-s['node']['xMeters']), F(-s['node']['zMeters']))
        return next(o for o in sectors if contains(o, across))
    def edge(p):
        return min(half - abs(float(p[0])), half - abs(float(p[1])))
    def isClear(p, r):
        if edge(p) - float(r) < float(gap): return False
        for a in ores:
            d = dist(p, a['pos'])
            if F(F(d - r) - a['r']) < gap or d < spacing: return False
        for fp, fr in fields:
            if F(F(dist(p, fp) - r) - fr) < gap: return False
        return all(not (F(dist(p, s) - r) < gap) for s in starts)
    rnd = Random(seed ^ 0x9e3779b97f4a7c15)
    for s in sectors:
        if not s.get('kind'): continue
        mi = mirror(s)
        if mi['id'] < s['id']: continue
        own = mi['id'] == s['id']
        node = (F(s['node']['xMeters']), F(s['node']['zMeters']))
        for rule in kinds[s['kind']]['ore']:
            r = F(rule['radiusMeters']); inset = F(border + r)
            w = F(F(F(s['maxXMeters']) - F(s['minXMeters'])) - F(F(2.0) * inset))
            d = F(F(F(s['maxZMeters']) - F(s['minZMeters'])) - F(F(2.0) * inset))
            draws = rule['count'] // 2 if own else rule['count']
            for _ in range(draws):
                placed = False
                for attempt in range(10000):
                    ux = F(rnd.unit()); uz = F(rnd.unit())
                    p = (F(F(F(s['minXMeters']) + inset) + F(ux * w)), F(F(F(s['minZMeters']) + inset) + F(uz * d)))
                    q = (F(-p[0]), F(-p[1]))
                    if dist(p, node) < nodeClear or not isClear(p, r): continue
                    apart = dist(p, q)
                    if F(apart - F(F(2.0) * r)) < gap or apart < spacing or not isClear(q, r): continue
                    ores.append(dict(pos=p, r=r, yld=rule['yield'], seeded=True, sector=s['name']))
                    ores.append(dict(pos=q, r=r, yld=rule['yield'], seeded=True, sector=mi['name']))
                    placed = True; break
                assert placed, s['name']
    def drawing(kind):
        return [s for s in sectors if s.get('kind') == kind and mirror(s)['id'] >= s['id']]
    outposts = []; taken = []
    for rule in m['outposts']:
        free = [s for s in drawing(rule['kind']) if s['id'] not in taken]
        for _ in range(rule['count']):
            i = rnd.below(len(free)); s = free.pop(i); taken.append(s['id'])
            outposts.append((s['name'], rule['outpost']))
            mi = mirror(s)
            if mi['id'] != s['id']: outposts.append((mi['name'], rule['outpost']))
    derelicts = []; salvaged = []; pairs = []
    def isClearOfAll(p, r):
        if edge(p) - float(r) < float(gap): return False
        apart = lambda o, orad: F(F(dist(p, o) - r) - orad) >= gap
        return (all(apart(a['pos'], a['r']) for a in ores) and all(apart(fp, fr) for fp, fr in fields) and
                all(apart(d['pos'], d['r']) for d in derelicts) and all(apart(s, F(0)) for s in starts))
    for rule in m['derelicts']:
        free = [s for s in drawing(rule['kind']) if s['id'] not in salvaged]
        for _ in range(rule['count']):
            i = rnd.below(len(free)); s = free.pop(i); salvaged.append(s['id'])
            r = F(rule['radiusMeters']); inset = F(border + r)
            w = F(F(F(s['maxXMeters']) - F(s['minXMeters'])) - F(F(2.0) * inset))
            d = F(F(F(s['maxZMeters']) - F(s['minZMeters'])) - F(F(2.0) * inset))
            node = (F(s['node']['xMeters']), F(s['node']['zMeters']))
            placed = False
            for attempt in range(10000):
                p = (F(F(F(s['minXMeters']) + inset) + F(F(rnd.unit()) * w)), F(F(F(s['minZMeters']) + inset) + F(F(rnd.unit()) * d)))
                q = (F(-p[0]), F(-p[1]))
                if dist(p, node) < nodeClear or not isClearOfAll(p, r) or F(dist(p, q) - F(F(2.0) * r)) < gap or not isClearOfAll(q, r):
                    continue
                pairs.append(len(derelicts))
                mi = mirror(s)
                derelicts.append(dict(pos=p, r=r, ore=rule['ore'], hull=rule['hull'], sector=s['name'], topic=None))
                derelicts.append(dict(pos=q, r=r, ore=rule['ore'], hull=rule['hull'], sector=mi['name'], topic=None))
                placed = True; break
            assert placed
    researching = round(len(pairs) * m['derelictResearchPercent'] / 100.0)
    names = {t['id']: t['name'] for t in tun['research']}
    for _ in range(researching):
        i = rnd.below(len(pairs)); first = pairs.pop(i)
        topic = topics[rnd.below(len(topics))]
        derelicts[first]['topic'] = derelicts[first + 1]['topic'] = names[topic]
    return ores, outposts, derelicts

if __name__ == '__main__':
    selftest(); print('RandomTests pinned draws: reproduced')
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 342
    root = '/home/user/Outpost.Commander/OutpostCommander/Assets/'
    ores, outposts, derelicts = place(seed, root + 'Map.json', root + 'Tuning.json')
    print('seed', seed, 'ore asteroids', len(ores), 'seeded', sum(a['seeded'] for a in ores))
    from collections import Counter
    print('per sector', dict(Counter(a['sector'] for a in ores if a['seeded'])))
    print('outposts', outposts)
    for d in derelicts: print('derelict', d['sector'], '(%.1f, %.1f)' % (float(d['pos'][0]), float(d['pos'][1])), d['ore'], d['hull'], d['topic'])
    json.dump(dict(ores=[dict(x=float(a['pos'][0]), z=float(a['pos'][1]), r=float(a['r']), yld=a['yld'], sector=a['sector']) for a in ores],
                   outposts=outposts, derelicts=[dict(x=float(d['pos'][0]), z=float(d['pos'][1]), r=float(d['r']), ore=d['ore'], sector=d['sector'], topic=d['topic']) for d in derelicts]),
              open('/home/user/Outpost.Commander/DesignReference/_work/V/v2/seed%d.json' % seed, 'w'), indent=1)
