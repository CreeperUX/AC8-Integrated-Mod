"""HUD texture candidates in the existing native style: cyan (88,203,225) strokes with a dark outline, 128x128 RGBA,
drawn 4x supersampled. Writes candidates to lua/ui-native/candidates/ and a labelled preview sheet."""
from PIL import Image,ImageDraw,ImageFont,ImageFilter
import math,os
HERE=os.path.dirname(os.path.abspath(__file__))
OUT=os.path.join(HERE,'ui-native','candidates');os.makedirs(OUT,exist_ok=True)
CYAN=(88,203,225,255);DARK=(0,0,0,230);S=4;N=128*S
def canvas():return Image.new('RGBA',(N,N),(0,0,0,0))
def finish(layers):
    # layers: list of (draw_fn) each drawing shapes with given colour/width offset; outline first then cyan
    im=canvas()
    for col,extra in ((DARK,OUT_EXTRA),(CYAN,0)):
        d=ImageDraw.Draw(im)
        for fn in layers:fn(d,col,extra)
    return im.resize((128,128),Image.LANCZOS)
c=N/2
def ring(r,w):
    def f(d,col,ex):d.ellipse([c-r-(w+ex)/2,c-r-(w+ex)/2,c+r+(w+ex)/2,c+r+(w+ex)/2],outline=col,width=int(w+ex))
    return f
def line(x0,y0,x1,y1,w):
    def f(d,col,ex):
        d.line([x0,y0,x1,y1],fill=col,width=int(w+ex))
        rr=(w+ex)/2
        for x,y in ((x0,y0),(x1,y1)):d.ellipse([x-rr,y-rr,x+rr,y+rr],fill=col)
    return f
def dot(r):
    def f(d,col,ex):rr=r+ex/2;d.ellipse([c-rr,c-rr,c+rr,c+rr],fill=col)
    return f
def arc(r,w,a0,a1):
    def f(d,col,ex):
        ww=w+ex;d.arc([c-r-ww/2,c-r-ww/2,c+r+ww/2,c+r+ww/2],a0,a1,fill=col,width=int(ww))
        for a in (a0,a1):
            x=c+r*math.cos(math.radians(a));y=c+r*math.sin(math.radians(a));rr=ww/2;d.ellipse([x-rr,y-rr,x+rr,y+rr],fill=col)
    return f
def poly(points,w,closed=True):
    def f(d,col,ex):
        pts=points+([points[0]] if closed else [])
        d.line(pts,fill=col,width=int(w+ex),joint='curve')
        rr=(w+ex)/2
        for x,y in pts:d.ellipse([x-rr,y-rr,x+rr,y+rr],fill=col)
    return f
# Measured on the shipped ring.png: ~3.5 px cyan core, ~2 px dark outline each side, stroke centre radius 47.5 px.
W=4*S;R=47.5*S;OUT_EXTRA=4*S
pol=lambda r,a:(c+r*math.cos(math.radians(a)),c+r*math.sin(math.radians(a)))
designs={}
# --- reference reticle (War Thunder-style gun cross: four arms with a central gap) ---
g0,g1=8*S,28*S
designs['bore-A']=('参考准星 A\n十字，中心留空（WT 风格）',[line(c-g1,c,c-g0,c,W),line(c+g0,c,c+g1,c,W),line(c,c-g1,c,c-g0,W),line(c,c+g0,c,c+g1,W)])
designs['bore-B']=('参考准星 B\n十字 + 中心点',[line(c-g1,c,c-g0-3*S,c,W),line(c+g0+3*S,c,c+g1,c,W),line(c,c-g1,c,c-g0-3*S,W),line(c,c+g0+3*S,c,c+g1,W),dot(4*S)])
# --- HMD mouse-ring candidates (must not read as a cross) ---
designs['hmd-1']=('头瞄圆环 1\n外侧斜向刻度',[ring(R,W)]+[line(*pol(R+7*S,a),*pol(R+19*S,a),W) for a in (45,135,225,315)])
designs['hmd-2']=('头瞄圆环 2\n双圈',[ring(R,W),ring(R-13*S,4*S)])
designs['hmd-3']=('头瞄圆环 3\n圆环 + 四角括号',[ring(R,W+.25*S)]+[poly([pol(R+11*S,a-14),pol(R+11*S,a),pol(R+11*S,a+14)],W,False) for a in (45,135,225,315)])
designs['hmd-4']=('头瞄圆环 4\n四段圆弧 + 中心点',[arc(R,W,a+12,a+78) for a in (0,90,180,270)]+[dot(4*S)])
designs['hmd-5']=('头瞄圆环 5\n菱形框',[poly([pol(R+6*S,a) for a in (270,0,90,180)],W)])
designs['hmd-6']=('头瞄圆环 6\n圆环 + 顶部三角',[ring(R-8*S,W),poly([(c-9*S,c-R-12*S),(c+9*S,c-R-12*S),(c,c-R+1*S)],W)])
designs['ring']=('当前普通圆环\n（参照，不改）',[ring(R,W)])
designs['ring-hmd-old']=('当前头瞄圆环\n（参照，将被替换）',[ring(R,W)]+[line(*pol(R-9*S,a),*pol(R-21*S,a),W) for a in (0,90,180,270)])
imgs={}
for k,(label,layers) in designs.items():
    im=finish(layers);imgs[k]=im
    if not k.startswith(('ring','ring-hmd-old')):im.save(os.path.join(OUT,k+'.png'))
# --- preview sheet: each candidate at in-game size (~60 px at 1080p) on sky and ground, plus a 2x close-up ---
font=None
for f in ['C:/Windows/Fonts/msyh.ttc','C:/Windows/Fonts/simhei.ttf']:
    if os.path.exists(f):font=ImageFont.truetype(f,20);small=ImageFont.truetype(f,16);break
order=['ring','ring-hmd-old','bore-A','bore-B','hmd-1','hmd-2','hmd-3','hmd-4','hmd-5','hmd-6']
cols=5;cw,ch=330,320;sheet=Image.new('RGB',(cols*cw,((len(order)+cols-1)//cols)*ch+70),(24,26,30))
d=ImageDraw.Draw(sheet);d.text((16,14),'AC8 HUD 候选：参考准星（WT 风格十字）与头瞄模式圆环（实际大小 + 2 倍放大；上=天空背景，下=地面背景）',fill=(230,230,230),font=font)
for i,k in enumerate(order):
    x0=(i%cols)*cw;y0=70+(i//cols)*ch
    sky=Image.new('RGB',(cw-20,110),(118,150,190));gnd=Image.new('RGB',(cw-20,110),(58,64,52))
    for bg,yy in ((sky,y0+40),(gnd,y0+160)):
        g=bg.copy();im=imgs[k]
        small_im=im.resize((60,60),Image.LANCZOS);big=im.resize((120,120),Image.LANCZOS)
        g.paste(small_im,(30,25),small_im);g.paste(big,(150,-5),big)
        sheet.paste(g,(x0+10,yy))
    d.multiline_text((x0+12,y0-6),designs[k][0],fill=(235,235,235),font=small,spacing=2)
sheet.save(os.path.join(HERE,'ui-native','candidates','preview.png'))
print('ok',list(designs))
