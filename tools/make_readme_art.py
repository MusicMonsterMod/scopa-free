#!/usr/bin/env python3
"""Compose pixel-sharp README graphics at the game's native 792px width."""
from pathlib import Path
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parent.parent
out=root/'docs/images'
out.mkdir(parents=True,exist_ok=True)
atlas=Image.open(root/'assets/winfont.bmp').convert('L')
widths=(root/'assets/winfont-widths.bin').read_bytes()
WIDTH=792

def text_width(value,scale=1):
    return sum(widths[ord(ch)] for ch in value)*scale

def text(im,x,y,value,color='white',scale=1):
    for ch in value:
        c=ord(ch);w=widths[c]
        glyph=atlas.crop((c%16*24,c//16*16,c%16*24+w,c//16*16+13)).resize((w*scale,13*scale),Image.Resampling.NEAREST)
        im.paste(color,(x,y,x+w*scale,y+13*scale),glyph);x+=w*scale

def card(im,index,x,y,scale=1):
    a=Image.open(root/f'assets/card{index:02}.bmp').convert('RGB')
    a=a.resize((a.width*scale,a.height*scale),Image.Resampling.NEAREST)
    im.paste(a,(x,y))

# Banner: same width as the game window so GitHub/HTML never downscales the bitmap font.
banner=Image.new('RGB',(WIDTH,280),'#008080');d=ImageDraw.Draw(banner)
d.rectangle((0,0,WIDTH-1,18),fill='#c0c0c0');text(banner,8,3,'Scopa Free - Native Linux',color='black')
d.rectangle((0,19,WIDTH-1,20),fill='white')
text(banner,24,40,'SCOPA FREE',scale=3)
text(banner,24,92,'Forty cards. Four suits. One clean sweep.')
text(banner,24,118,'A teal table. Red backs. Bitmap letters.')
text(banner,24,144,'Open the AppImage and play.')
d.rectangle((24,188,252,219),fill='#c0c0c0')
d.line((24,219,24,188,252,188),fill='white',width=1)
d.line((24,219,252,219,252,188),fill='black',width=1)
text(banner,36,196,'C++ / SDL2 / AppImage',color='black')
for i,idx in enumerate([0,19,32]):card(banner,idx,520+i*82,48)
text(banner,520,228,'No installer. Just play.')
banner.save(out/'banner.png')

# Deck gallery: 2×2 suit grid keeps native 71×125 cards inside 792px.
gallery=Image.new('RGB',(WIDTH,430),'#008080')
text(gallery,24,16,'THE ITALIAN DECK',scale=2)
suits=[('Cups',[0,6,12]),('Coins / Denari',[13,19,25]),('Batons',[26,32,38]),('Swords',[39,45,51])]
for i,(label,ids) in enumerate(suits):
    col,row=i%2,i//2
    x=24+col*384
    y=56+row*170
    text(gallery,x,y,label)
    for j,index in enumerate(ids):card(gallery,index,x+j*82,y+22)
    text(gallery,x,y+154,'Ace / Seven / King')
text(gallery,24,400,'Card faces by Michael P. Reed.')
gallery.save(out/'cards.png')
print(f'Wrote {WIDTH}px README art to {out}')
