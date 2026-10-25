# Collect every color the screen frames' game layer uses and map each to the code's tokens (sRGB of the linear constants).
import os, re, json, sys, collections
sys.path.insert(0, os.path.dirname(__file__))
from frames import frame, SRC
code=json.load(open(os.path.join(os.path.dirname(__file__),'code_colors.json')))
byhex=collections.defaultdict(list)
for k,v in code.items(): byhex[v['hex']].append(k)
def norm(c):
    c=c.strip()
    m=re.match(r'#([0-9a-fA-F]{6})$',c)
    if m: return '#'+m.group(1).upper(), 1.0
    m=re.match(r'#([0-9a-fA-F]{3})$',c)
    if m: return '#'+''.join(x*2 for x in m.group(1)).upper(),1.0
    m=re.match(r'rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*([\d.]+))?\)',c)
    if m: return '#%02X%02X%02X'%tuple(int(m.group(i)) for i in (1,2,3)), float(m.group(4) or 1)
    return None
uses=collections.defaultdict(lambda: collections.Counter())
for f in sorted(os.listdir(SRC)):
    if not f.startswith('screen-'): continue
    for e in frame(f):
        if e['layer']!='game': continue
        vals=[]
        st=e['style']
        for k in ('background','color','border','border-top','border-bottom','border-left','border-right','outline','box-shadow','text-shadow'):
            if k in st: vals+= re.findall(r'#[0-9a-fA-F]{3,6}\b|rgba?\([^)]*\)',st[k])
        for k in ('stroke','fill'):
            if k in e['attrs'] and e['attrs'][k] not in ('none',): vals.append(e['attrs'][k])
        role = 'text' if e['text'].strip() else e['tag']
        for v in vals:
            n=norm(v)
            if n: uses[n][(f,role)]+=1
out=[]
for (hx,a),c in sorted(uses.items()):
    toks=byhex.get(hx,[])
    frames=sorted(set(k[0] for k in c))
    roles=collections.Counter(k[1] for k in c)
    out.append((hx,a,toks,len(frames),dict(roles)))
    print(f"{hx} a={a:<5} tokens={toks} frames={len(frames)} roles={dict(roles)}")
