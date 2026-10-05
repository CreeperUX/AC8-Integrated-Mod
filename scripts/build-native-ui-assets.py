from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import json
root=Path(__file__).resolve().parents[1];out=root/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts/ui-native';out.mkdir(exist_ok=True)
fonts=root/'package-template/ui/creeperux/fonts';tokens=json.loads((root/'package-template/ui/creeperux/tokens.json').read_text())
def color(k):return tokens['color'][k]['dark']
mono=lambda n:ImageFont.truetype(str(fonts/'share-tech-mono-400-latin.ttf'),n*2)
title=lambda n:ImageFont.truetype(str(fonts/'chakra-petch-600-latin.ttf'),n*2)
sans=lambda n:ImageFont.truetype('segoeui.ttf',n*2)
def canvas(w,h):
 im=Image.new('RGBA',(w*2,h*2));return im,ImageDraw.Draw(im)
def box(d,b,fill,r=0,outline=None):d.rounded_rectangle(tuple(x*2 for x in b),radius=r*2,fill=fill,outline=outline,width=2)
def text(d,x,y,s,font,fill):d.text((x*2,y*2),s,font=font,fill=fill,anchor='lt')
def save(im,n):im.save(out/(n+'.png'))
# Ring and HMD state, supersampled at4x for crisp edges in native UMG.
for hmd in [False,True]:
 im=Image.new('RGBA',(512,512));d=ImageDraw.Draw(im)
 d.ellipse((48,48,464,464),outline=(4,12,16,235),width=32)
 d.ellipse((57,57,455,455),outline=(82,190,211,255),width=14)
 if hmd:
  for a,b in [((256,84),(256,120)),((256,392),(256,428)),((84,256),(120,256)),((392,256),(428,256))]:
   d.line((a,b),fill=(4,12,16,235),width=28);d.line((a,b),fill=(82,190,211,255),width=12)
 save(im.resize((128,128),Image.Resampling.LANCZOS),'ring-hmd' if hmd else 'ring')
im=Image.new('RGBA',(64,24));d=ImageDraw.Draw(im);d.rounded_rectangle((2,2,62,22),radius=8,fill=(4,12,16,230));d.rounded_rectangle((5,8,59,16),radius=4,fill=(82,190,211,255));save(im,'tick')
print('Generated ring, ring-hmd and tick textures')
