#!/usr/bin/env python3
"""Canonical approved coat artwork -> native Swift/C++ meshes (stdlib only)."""
import argparse
from pathlib import Path
import math
ROOT=Path(__file__).resolve().parents[1]
MILK=0xF2EEE4; GINGER=0xD8AA73; DARK=0x363536

def curve(a,b,c,d,t):
    u=1-t
    return (u*u*u*a[0]+3*u*u*t*b[0]+3*u*t*t*c[0]+t*t*t*d[0],u*u*u*a[1]+3*u*u*t*b[1]+3*u*t*t*c[1]+t*t*t*d[1])
def shape(marks,head,start,curves,rgb,alpha=1):
    pts=[start]; last=start
    for end,c1,c2 in curves:
        pts += [curve(last,c1,c2,end,i/12) for i in range(1,13)];last=end
    marks.append((head,0,rgb,alpha,pts))
def ellipse(marks,head,rect,rgb):
    x,y,w,h=rect
    pts=[(x+w/2+w/2*math.cos(i*math.tau/64),y+h/2+h/2*math.sin(i*math.tau/64)) for i in range(64)]
    marks.append((head,0,rgb,1,pts))
def rect(marks,head,x,y,w,h,rgb):marks.append((head,0,rgb,1,[(x,y),(x+w,y),(x+w,y+h),(x,y+h)]))
def stroke(marks,head,a,c,b,width,rgb,alpha):
    pts=[((1-t)**2*a[0]+2*(1-t)*t*c[0]+t*t*b[0],(1-t)**2*a[1]+2*(1-t)*t*c[1]+t*t*b[1]) for t in [i/16 for i in range(17)]]
    marks.append((head,width,rgb,alpha,pts))
def artwork(silver):
    m=[]
    if silver:
        ellipse(m,False,(-36,-81,72,130),MILK)
        for side in [-1,1]:
            stroke(m,False,(side*49,-41),(side*37,-44),(side*33,-35),3.5,DARK,.75)
            stroke(m,False,(side*49,-20),(side*39,-25),(side*35,-14),3.2,DARK,.75)
    else:
        # Keep the rounded inner edge; extend the hidden outer edge past the
        # silhouette so head turns and breathing cannot expose a white sliver.
        shape(m,False,(51,-73),[
            ((29,-38),(23,-76),(21,-59)),((47,4),(35,-16),(20,4)),
            ((85,20),(60,12),(75,20)),((85,-95),(85,-15),(85,-70)),
            ((51,-73),(72,-95),(61,-82))],GINGER)
        ellipse(m,False,(-59,-65,28,51),GINGER)
        # Skull-attached orange volume: its edges lie beyond the clipped cat silhouette.
        # The curved nape stays orange when the facial markings turn away.
        shape(m,2,(-100,-100),[
            ((100,-100),(-30,-100),(30,-100)),((100,14),(100,-70),(100,-15)),
            ((-100,14),(45,55),(-45,55)),((-100,-100),(-100,-15),(-100,-70))],GINGER)
    col=DARK if silver else 0xA57543;alpha=.86 if silver else .48
    shape(m,True,(-13,-33),[((-9,-16),(-17,-27),(-7,-23)),((-5,-30),(-11,-22),(-3,-26)),((-13,-33),(-7,-30),(-10,-32))],col,alpha)
    shape(m,True,(0,-34),[((-2,-22),(-5,-29),(3,-25)),((1,-12),(-5,-18),(0,-15)),((5,-24),(5,-18),(7,-20)),((2,-34),(2,-29),(6,-31))],col,alpha)
    shape(m,True,(14,-33),[((11,-18),(18,-27),(9,-23)),((7,-31),(13,-24),(6,-25))],col,alpha)
    for side in [-1,1]:
        shape(m,True,(side*22,-30),[((side*16,-15),(side*24,-23),(side*18,-17)),((side*18,-30),(side*18,-21),(side*20,-24))],col,alpha)
        stroke(m,True,(side*43,3),(side*35,8),(side*29,7),3.2 if silver else 2.1,col,alpha)
        stroke(m,True,(side*43,14),(side*36,16),(side*31,15),2.8 if silver else 1.8,col,alpha)
    if silver:
        top=-16; hw=6
        shape(m,True,(-hw,top),[
            ((-9,7),(-6,top+(5)),(-6,0)),
            ((-30,18),(-15,13),(-24,14)),((0,35),(-30,28),(-14,35)),
            ((30,18),(14,35),(30,28)),((9,7),(24,14),(15,13)),
            ((hw,top),(6,0),(6,top+(5)))],MILK)
    else:
        # The white muzzle becomes a rounded bib extending into the white chest.
        shape(m,True,(0,-24),[
            ((-9,7),(-5,-16),(-4,0)),((-29,20),(-15,13),(-25,14)),
            ((-42,25),(-33,23),(-38,25)),((-58,28),(-47,25),(-54,25)),
            ((-70,48),(-64,31),(-68,41)),((70,48),(-30,70),(30,70)),
            ((58,28),(68,41),(64,31)),((42,25),(54,25),(47,25)),
            ((29,20),(38,25),(33,23)),((9,7),(25,14),(15,13)),
            ((0,-24),(4,0),(5,-16))],MILK)
    if not silver:
        # Preserve the original continuous white neck/chest undercoat and head
        # background. Only decorative head marks and the actual muzzle are foreground.
        undercoat=[mark for mark in m if mark[0]==2 or (mark[0] and mark[2]==MILK)]
        body=[mark for mark in m if not mark[0]]
        patterns=[mark for mark in m if mark[0] and mark[0]!=2 and mark[2]!=MILK]
        m=undercoat+body+patterns
        # Complete opaque round muzzle; no intersecting ellipse clip and no new
        # colored head base below the chin. The original white bib joins the chest.
        shape(m,True,(0,-24),[
            ((-9,7),(-5,-16),(-4,0)),((-29,20),(-15,13),(-25,14)),
            ((0,35),(-30,29),(-14,35)),((29,20),(14,35),(30,29)),
            ((9,7),(25,14),(15,13)),((0,-24),(4,0),(5,-16))],MILK)
        # Restore the existing white bib only where the turned cheek occludes body
        # spots. Clip this coverage to that original bib, preserving the face outline.
        shape(m,4,(0,-24),[
            ((-9,7),(-5,-16),(-4,0)),((-29,20),(-15,13),(-25,14)),
            ((-39,26),(-34,23),(-37,24)),((0,40),(-38,35),(-15,40)),
            ((39,26),(15,40),(38,35)),((29,20),(37,24),(34,23)),
            ((9,7),(25,14),(15,13)),((0,-24),(4,0),(5,-16))],MILK)
    return m

def n(v):
    s=f'{v:.5f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0','') else s

def outputs():
    skins=[('shorthair',artwork(True)),('orangeWhite',artwork(False))]
    swift='// Generated by scripts/generate-coat-patterns.py; edit the canonical artwork there.\nimport CoreGraphics\nstruct CoatMark {\n    let head: Bool\n    let skull: Bool\n    let clipToBib: Bool\n    let width: CGFloat\n    let rgb: Int\n    let alpha: CGFloat\n    let points: [CGPoint]\n}\nenum CoatPatterns {\n'
    cpp='// Generated by scripts/generate-coat-patterns.py; shared by Windows and Linux.\n#pragma once\n#include <vector>\nstruct CoatPoint { float x, y; };\nstruct CoatMark { bool head; bool skull; bool clipToBib; float width; unsigned int rgb; float alpha; std::vector<CoatPoint> points; };\n'
    for skin,marks in skins:
        swift+=f'    static let {skin}: [CoatMark] = [\n'
        cpp+=f'inline const std::vector<CoatMark>& {skin}Marks() {{\n    static const std::vector<CoatMark> marks = {{\n'
        for head,w,rgb,a,pts in marks:
            swift+=f'        CoatMark(head: {str(bool(head)).lower()}, skull: {str(head == 2).lower()}, clipToBib: {str(head == 4).lower()}, width: {n(w)}, rgb: 0x{rgb:06X}, alpha: {n(a)}, points: [\n'
            swift+='            '+', '.join(f'CGPoint(x: {n(x)}, y: {n(y)})' for x,y in pts)+']),\n'
            cpp+=f'        {{{str(bool(head)).lower()}, {str(head == 2).lower()}, {str(head == 4).lower()}, {float(w):.5f}f, 0x{rgb:06X}, {float(a):.5f}f, {{\n'
            cpp+='            '+', '.join('{'+f'{float(x):.5f}f, {float(y):.5f}f'+'}' for x,y in pts)+'}},\n'
        swift+='    ]\n';cpp+='    };\n    return marks;\n}\n'
    swift+='    static func marks(for coat: CatCoatTheme) -> [CoatMark] {\n        switch coat {\n        case .shorthair: return shorthair\n        case .orangeWhite: return orangeWhite\n        default: return []\n        }\n    }\n}\n'
    files={ROOT/'Sources/CoatPatterns.swift':swift,ROOT/'shared/CoatPatterns.hpp':cpp}
    files.update(templates(dict(skins)))
    return files

# --- .kurocat templates of the four built-in coats (docs/skin-format.md) ---------------------------
# Colours are the Linux/Cairo renderer's exact doubles rounded to #RRGGBB(AA); the apps keep the exact
# values in their built-in SkinSpecs; imported templates render within 3/255 (premultiplied) of them.
def hx(r,g,b,a=1.0):
    c=''.join(f'{round(v*255):02X}' for v in (r,g,b))
    return '#'+c+('' if a>=1 else f'{round(a*255):02X}')
def hexrgb(rgb,a=1.0): return hx(((rgb>>16)&255)/255,((rgb>>8)&255)/255,(rgb&255)/255,a)
BLACK=hx(.085,.085,.095); MINT=hx(.82,.92,.89,.76); WHISKER=hx(.88,.90,.92,.68)
LIGHT_WHISKER=hexrgb(0x8D8170,.68); LIGHT_CREASE=hexrgb(0x8B8171,.60)
POINT=hx(.208,.149,.122)
PALETTES={
 'black':('经典小黑猫',{'body':BLACK,'legs':BLACK,'paws':BLACK,'earOuter':BLACK,'tail':BLACK,'whiskers':WHISKER,'crease':MINT},
          {'build':'standard'},'dome',None),
 'siamese':('挖煤暹罗猫',{'body':hx(.965,.937,.894),'bodyShade':hx(.918,.859,.784),'points':POINT,'pointsMid':hx(.416,.314,.255),
          'legs':POINT,'paws':POINT,'earOuter':POINT,'tail':POINT,'earInner':hx(.365,.275,.228),'outline':hx(.68,.59,.50,.42),
          'crease':hx(.553,.471,.404,.54),'whiskers':hx(.478,.388,.325,.62),'nose':hx(.14,.09,.08,.95),'mouth':hx(.78,.66,.56,.55)},
          {'build':'sleek'},'dome',None),
 'shorthair':('美短（银虎斑加白）',{'body':hexrgb(0xC2C5C1),'legs':hexrgb(0xC2C5C1),'earOuter':hexrgb(0xC2C5C1),'tail':hexrgb(0xC2C5C1),
          'belly':hexrgb(MILK),'paws':hexrgb(MILK),'stripes':hexrgb(0x55595A),'whiskers':LIGHT_WHISKER,'crease':LIGHT_CREASE},
          {'build':'standard','pawStagger':0},'plane',0x55595A),
 'britishShorthair':('英短蓝猫（灰毛橘黄瞳）',{'body':'#3A3F47','legs':'#3A3F47','earOuter':'#3A3F47','tail':'#3A3F47','paws':'#3A3F47','earInner':'#B9A4A4','nose':'#9C7F82','iris':'#E8A21A','whiskers':'#D8DCE0AD','crease':'#181B20C0'},
          {'build':'standard','pawStagger':0},'dome',None),
 'orangeWhite':('橘白猫',{'body':hexrgb(MILK),'legs':hexrgb(GINGER),'earOuter':hexrgb(GINGER),'tail':hexrgb(GINGER),
          'belly':hexrgb(MILK),'paws':hexrgb(MILK),'stripes':hexrgb(0xA57543),'whiskers':LIGHT_WHISKER,'crease':LIGHT_CREASE},
          {'build':'standard','pawStagger':0},'dome',0xA57543),
}
def templates(artworks):
    import json
    out={}
    for key,(name,palette,shape,surface,rings) in PALETTES.items():
        marks=[]
        for head,w,rgb,a,pts in artworks.get(key,[]):
            m={'space':'skull' if head==2 else ('head' if head else 'body'),'kind':'stroke' if w else 'polygon','color':hexrgb(rgb)}
            if head==4: m['clipToBib']=True
            if a!=1: m['alpha']=float(n(a))
            if w: m['width']=float(n(w))
            m['points']=[[float(n(x)),float(n(y))] for x,y in pts]
            marks.append(m)
        if rings is not None:
            for x in (50,78): marks.append({'space':'tail','kind':'stroke','color':hexrgb(rings),'alpha':0.5,'width':3.5,'points':[[x,8],[x,-8]]})
        doc={'formatVersion':1,'name':name,'author':'KuroCat','palette':palette,'shape':shape,'headSurface':surface,'idleLegs':'chest','marks':marks}
        text=json.dumps(doc,ensure_ascii=False,indent=1)
        # keep each point list on one line so the templates stay readable
        import re
        text=re.sub(r'\[\n\s*(-?[\d.]+),\n\s*(-?[\d.]+)\n\s*\]',r'[\1, \2]',text)
        out[ROOT/'skins'/f'{key}.kurocat']=text+'\n'
    return out
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');args=p.parse_args()
    for path,text in outputs().items():
        if args.check:
            if not path.exists() or path.read_text()!=text:raise SystemExit(f'Generated artwork is stale: {path}')
        else:path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text)
    print('Shared coat artwork is consistent' if args.check else 'Generated native coat artwork')
