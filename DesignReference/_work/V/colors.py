# Convert every linear color constant in the code to the sRGB hex an sRGB render-target view writes, and compare with the
# reference's token table (visual-colors.dc.html).
import re, json, sys, os
ROOT='/home/user/Outpost.Commander'
def enc(c):
    c=max(0.0,min(1.0,c))
    return 12.92*c if c<=0.0031308 else 1.055*(c**(1/2.4))-0.055
def hexof(r,g,b):
    return '#%02X%02X%02X'%tuple(int(round(enc(x)*255)) for x in (r,g,b))
files=['GameApp/Hud.cpp','GameApp/GameClient.cpp','GameApp/CombatEffects.cpp','GameApp/SceneShading.h','OutpostCommander/WinMain.cpp']
consts={}
for f in files:
    for i,l in enumerate(open(os.path.join(ROOT,f),encoding='utf-8'),1):
        m=re.search(r'XMFLOAT[34]\s+([A-Z_]+)\s*\{([^}]*)\}',l)
        if m:
            vals=[float(v.strip().rstrip('f')) for v in m.group(2).split(',')]
            if m.group(1) in ('UP','TOWARD_KEY'): continue
            consts[m.group(1)]=(f,i,vals,hexof(*vals[:3]))
        m=re.search(r'XMFLOAT4\s+([A-Z_]+)\s*=\s*([A-Z_]+);',l)
        if m and m.group(2) in consts:
            src=consts[m.group(2)]
            consts[m.group(1)]=(f,i,src[2],src[3])
if __name__=='__main__':
    # reference table
    txt=open(os.path.join(ROOT,'DesignReference/_work/V/text/visual-colors.txt')).read()
    ref=dict(re.findall(r'^([A-Z_]+(?: \([a-z]+\))?)#([0-9A-F]{6})',txt,re.M))
    ref.update({k:v for k,v in re.findall(r'^([A-Za-z]+)#([0-9A-F]{6})',txt,re.M)})
    for k,(f,i,v,h) in consts.items():
        r=ref.get(k)
        print(f"{k:24s} {f}:{i} {v} code={h} ref={'#'+r if r else '-'} {'OK' if r and '#'+r==h else 'DIFF'}")
    print('ref-only:',[k for k in ref if k not in consts])
    json.dump({k:{'file':f,'line':i,'linear':v,'hex':h} for k,(f,i,v,h) in consts.items()},open(os.path.join(ROOT,'DesignReference/_work/V/code_colors.json'),'w'),indent=1)
