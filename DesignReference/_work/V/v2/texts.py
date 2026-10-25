# V2: the game layer's texts of every screen frame, in reading order (top, then left), into texts/<frame>.txt.
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from frames import frame, SRC
OUT = os.path.join(os.path.dirname(__file__), 'texts'); os.makedirs(OUT, exist_ok=True)
for f in sorted(os.listdir(SRC)):
    if not f.startswith('screen-'): continue
    rows = []
    for e in frame(f):
        if e['layer'] != 'game': continue
        t = e['text'].strip()
        if not t: continue
        st = e['style']
        x = float(st.get('left', '0').replace('px', '') or 0); y = float(st.get('top', '0').replace('px', '') or 0)
        rows.append((round(y), round(x), st.get('font-size', ''), st.get('color', ''), t))
    rows.sort()
    with open(os.path.join(OUT, f.replace('.dc.html', '.txt')), 'w') as o:
        for r in rows: o.write('%5d %5d %6s %-8s %s\n' % r)
    # the aside (notes) as plain text
    import re, html
    raw = open(os.path.join(SRC, f), encoding='utf-8').read()
    m = re.search(r'<aside.*?</aside>', raw, re.S)
    if m:
        t = html.unescape(re.sub(r'<[^>]+>', ' ', m.group(0))); t = re.sub(r'\s+', ' ', t)
        with open(os.path.join(OUT, f.replace('.dc.html', '.aside.txt')), 'w') as o: o.write(t + '\n')
print('ok')
