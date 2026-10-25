# V2: read each in-match frame's minimap from the published HTML (sector washes, ore outlines, marks, crosses) and compare
# the ore outlines with the seed replay (seed342.json). Usage: python3 minimap.py
import json, os, re, sys
from collections import Counter
sys.path.insert(0, os.path.dirname(__file__))
from frames import frame, SRC
seed = json.load(open(os.path.join(os.path.dirname(__file__), 'seed342.json')))
COLS = 'ABCDE'
def hexs(c):
    return c.strip().upper()
NAMES = {'#455E75': 'own', '#674F54': 'enemy', '#5D5375': 'pirate', '#AD8427': 'ore', '#52382C': 'dry?', '#656569': 'field'}
def analyse(f):
    els = [e for e in frame(f) if e['layer'] == 'game']
    # the minimap's inner square: the MAP_COLOR fill
    inner = None
    for e in els:
        st = e['style']
        if st.get('background', '').upper() in ('#263749',) and 'width' in st:
            inner = tuple(float(st[k].replace('px', '')) for k in ('left', 'top', 'width', 'height')); break
    if inner is None: return None
    L, T, W, H = inner
    ppm = W / 10000.0; cell = W / 5
    def sector_of(x, y):
        c = int((x - L) // cell); r = 5 - int((y - T) // cell)
        return COLS[c] + str(r) if 0 <= c < 5 and 1 <= r <= 5 else '?'
    washes = {}; ores = []; fills = Counter(); others = []
    for e in els:
        st = e['style']
        try:
            x, y, w, h = (float(st[k].replace('px', '')) for k in ('left', 'top', 'width', 'height'))
        except Exception:
            continue
        if not (L - 1 <= x <= L + W + 1 and T - 1 <= y <= T + H + 1): continue
        bg = st.get('background', ''); bd = st.get('border', '')
        if abs(w - cell) < 0.05 and abs(h - cell) < 0.05 and bg and not bd:
            washes.setdefault(sector_of(x + 1, y + 1), []).append(bg.upper())
        elif bd and '2.0px solid' in bd or (bd and 'px solid' in bd and w < 20):
            col = bd.split()[-1].upper()
            cx, cy = x + w / 2, y + h / 2
            ores.append(((cx - L) / ppm - 5000, 5000 - (cy - T) / ppm, col, w))
        elif bg and w < 20:
            others.append((round(x, 2), round(y, 2), round(w, 2), round(h, 2), bg.upper(), sector_of(x + w / 2, y + h / 2)))
    return dict(inner=inner, washes=washes, ores=ores, others=others, ppm=ppm, sector_of=sector_of)

if __name__ == '__main__':
    exp = [(a['x'], a['z']) for a in seed['ores']]
    for f in sorted(os.listdir(SRC)):
        if not (f.startswith('screen-hud') or f.startswith('screen-window')): continue
        a = analyse(f)
        if a is None: print(f, 'no minimap'); continue
        wash = {k: ','.join(NAMES.get(c, c) for c in v) for k, v in sorted(a['washes'].items())}
        # match ore marks to the replay within 1.5 m-equivalent of a pixel
        tol = 0.6 / a['ppm']
        unmatched = [o for o in a['ores'] if not any(abs(o[0] - x) < tol and abs(o[1] - z) < tol for x, z in exp)]
        missing = [(x, z) for x, z in exp if not any(abs(o[0] - x) < tol and abs(o[1] - z) < tol for o in a['ores'])]
        cols = Counter(o[2] for o in a['ores'])
        print(f.replace('.dc.html', ''), '| ore outlines', len(a['ores']), dict(cols), '| unmatched', len(unmatched), 'missing', len(missing))
        print('   washes', wash)
        oc = Counter((o[4], o[2]) for o in a['others'])
        print('   small fills (color,width):', dict(oc))
