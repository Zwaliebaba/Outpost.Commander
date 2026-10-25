# Pick numeric facts at random (fixed seed) from the chapters' table rows, for check 1.
import os, re, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from frames import frame, SRC
from html.parser import HTMLParser
class Rows(HTMLParser):
    def __init__(s):
        super().__init__(); s.rows=[]; s.row=None; s.cell=None; s.skip=0
    def handle_starttag(s,t,a):
        if t in ('style','script'): s.skip+=1
        if t=='tr': s.row=[]
        if t in ('td','th') and s.row is not None: s.cell=''
    def handle_endtag(s,t):
        if t in ('style','script'): s.skip-=1
        if t in ('td','th') and s.row is not None and s.cell is not None: s.row.append(s.cell.strip()); s.cell=None
        if t=='tr' and s.row is not None: s.rows.append(s.row); s.row=None
    def handle_data(s,d):
        if s.skip: return
        if s.cell is not None: s.cell+=d
facts=[]
for f in sorted(os.listdir(SRC)):
    if not f.endswith('.html') or f.startswith('screen-') or f.startswith('assets-') or f in ('Main.dc.html','glossary-1.dc.html','glossary-2.dc.html'): continue
    p=Rows(); p.feed(open(os.path.join(SRC,f),encoding='utf-8').read())
    for r in p.rows:
        for i,c in enumerate(r):
            if i==0: continue
            if re.search(r'\d',c) and not re.match(r'^[A-Za-z]+\.(json|cpp|h|md)',c) and len(c)<200 and not re.search(r'\.(cpp|h|json|md)[:\s]',c):
                facts.append((f,r[0][:60],c[:160]))
random.seed(20261025)
pick=random.sample(facts,30)
for i,(f,k,v) in enumerate(pick,1):
    print(f"R{i}\t{f}\t{k}\t{v}")
print(len(facts),'facts', file=sys.stderr)
