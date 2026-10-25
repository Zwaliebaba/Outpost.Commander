# Convert each artboard to plain text (tables as pipe rows) for review.
import sys, os, re, html
from html.parser import HTMLParser
SRC = '/home/user/Outpost.Commander/DesignReference/canvas/project'
DST = '/home/user/Outpost.Commander/DesignReference/_work/V/text'
class P(HTMLParser):
    def __init__(s):
        super().__init__(); s.out=[]; s.skip=0; s.cell=False
    def handle_starttag(s,t,a):
        if t in ('style','script','helmet'): s.skip+=1
        if t in ('p','div','h1','h2','h3','h4','li','tr','br','header','section','table','pre','figcaption'): s.out.append('\n')
        if t=='li': s.out.append('- ')
        if t in ('td','th'): s.out.append(' | ')
        if t=='img':
            d=dict(a); s.out.append(f"[IMG {d.get('src')} alt={d.get('alt')}]")
    def handle_endtag(s,t):
        if t in ('style','script','helmet'): s.skip-=1
        if t in ('h1','h2','h3','h4'): s.out.append('\n')
    def handle_data(s,d):
        if s.skip<=0: s.out.append(d)
for f in sorted(os.listdir(SRC)):
    if not f.endswith('.html'): continue
    p=P(); p.feed(open(os.path.join(SRC,f),encoding='utf-8').read())
    txt=''.join(p.out)
    txt=re.sub(r'[ \t]+',' ',txt); txt=re.sub(r'\n\s*\n+','\n',txt)
    open(os.path.join(DST,f.replace('.dc.html','.txt')),'w').write(txt)
