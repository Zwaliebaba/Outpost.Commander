# Write each screen frame's game-layer elements as a compact TSV: kind x y w h color size weight text.
import os, sys, re
sys.path.insert(0, os.path.dirname(__file__))
from frames import frame, SRC
OUT=os.path.join(os.path.dirname(__file__),'frames')
def num(v):
    return v.replace('px','') if v else ''
for f in sorted(os.listdir(SRC)):
    if not f.startswith('screen-'): continue
    rows=[]
    pending_svg=None
    for e in frame(f):
        st=e['style']; L=e['layer']
        if L not in ('game','annot'): continue
        if e['tag']=='svg':
            pending_svg=(num(st.get('left')),num(st.get('top')),e['attrs'].get('width',''),e['attrs'].get('height',''))
            continue
        if e['tag'] in ('path','rect','line','circle','polygon','polyline'):
            col=e['attrs'].get('stroke') or e['attrs'].get('fill')
            x,y,w,h=pending_svg or ('','','','')
            rows.append((L,'svg:'+e['tag'],x,y,w,h,(e['attrs'].get('stroke','')+'/'+e['attrs'].get('fill','')),'','',''))
            continue
        bg=st.get('background',''); bd=st.get('border','')
        txt=e['text'].strip()
        kind='text' if txt else ('border' if bd else ('fill' if bg else 'div'))
        col=st.get('color','') if txt else (bd if bd else bg)
        rows.append((L,kind,num(st.get('left')),num(st.get('top')),num(st.get('width')),num(st.get('height')),col,num(st.get('font-size','')),st.get('font-weight',''),txt))
    with open(os.path.join(OUT,f.replace('.dc.html','.tsv')),'w') as o:
        for r in rows: o.write('\t'.join(str(x) for x in r)+'\n')
print('done')
