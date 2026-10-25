# Extract every positioned element of each screen frame (the game's layer and the annotation layer separately).
import re, os, json, sys
from html.parser import HTMLParser
SRC='/home/user/Outpost.Commander/DesignReference/canvas/project'
def style_of(s):
    d={}
    for part in s.split(';'):
        if ':' in part:
            k,v=part.split(':',1); d[k.strip()]=v.strip()
    return d
class P(HTMLParser):
    def __init__(s):
        super().__init__(); s.stack=[]; s.els=[]; s.layer=None; s.depth=0; s.cur=None
    def handle_starttag(s,t,a):
        a=dict(a); st=style_of(a.get('style','')); s.depth+=1
        if t in ('br','img','path','rect','line','circle','polygon','polyline','meta','link'): s.depth-=1
        if a.get('role')=='img' and 'aria-label' in a and s.layer is None:
            s.layer=('game',s.depth,a['aria-label'])
        elif a.get('aria-hidden')=='true' and t=='div' and s.layer and s.layer[0]=='game' and False:
            pass
        if t=='div' and a.get('aria-hidden')=='true' and 'pointer-events:none' in a.get('style',''):
            s.layer=('annot',s.depth,'')
        if t=='aside': s.layer=('aside',s.depth,'')
        el={'tag':t,'style':st,'attrs':{k:v for k,v in a.items() if k!='style'},'text':'','layer':s.layer[0] if s.layer else None}
        s.els.append(el); s.stack.append(el)
        if t in ('br','img','path','rect','line','circle','polygon','polyline','meta','link'): s.stack.pop()
    def handle_startendtag(s,t,a):
        a2=dict(a); st=style_of(a2.get('style',''))
        s.els.append({'tag':t,'style':st,'attrs':{k:v for k,v in a2.items() if k!='style'},'text':'','layer':s.layer[0] if s.layer else None})
    def handle_endtag(s,t):
        if t in ('br','img','path','rect','line','circle','polygon','polyline','meta','link'): return
        if s.layer and s.depth==s.layer[1]: s.layer=None
        s.depth-=1
        if s.stack: s.stack.pop()
    def handle_data(s,d):
        if s.stack: s.stack[-1]['text']+=d
def frame(f):
    p=P(); p.feed(open(os.path.join(SRC,f),encoding='utf-8').read()); return p.els
if __name__=='__main__':
    f=sys.argv[1]
    for e in frame(f):
        if e['layer']!=(sys.argv[2] if len(sys.argv)>2 else 'game'): continue
        st=e['style']
        keys=['left','top','width','height','background','border','color','font-size','font-weight','font-family']
        out={k:st[k] for k in keys if k in st}
        for k in ('stroke','fill','d','x','y','width','height'):
            if k in e['attrs']: out['@'+k]=e['attrs'][k]
        print(e['tag'], json.dumps(out)[:260], repr(e['text'][:80]) if e['text'].strip() else '')
