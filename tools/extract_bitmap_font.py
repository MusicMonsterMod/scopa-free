#!/usr/bin/env python3
"""Extract Wine's open-source MS Sans Serif bitmap strike, without smoothing."""
import struct
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parent.parent
def font_resource(path):
    d=path.read_bytes();ne=struct.unpack_from('<I',d,60)[0]
    p=ne+struct.unpack_from('<H',d,ne+36)[0];shift=struct.unpack_from('<H',d,p)[0];p+=2
    while True:
        kind,count=struct.unpack_from('<HH',d,p);p+=8
        if not kind:break
        for _ in range(count):
            off,size,_,rid,_,_=struct.unpack_from('<6H',d,p);p+=12
            if kind==0x8008:
                f=d[off<<shift:(off+size)<<shift]
                if struct.unpack_from('<H',f,68)[0]==8:return f
    raise RuntimeError('8pt font strike not found')
f=font_resource(Path('/usr/share/wine/fonts/sserife.fon'))
height=struct.unpack_from('<H',f,88)[0];first,last=f[95:97]
atlas=Image.new('RGB',(16*24,16*16),'black');widths=bytearray(256)
for code in range(first,last+1):
    width,off=struct.unpack_from('<HI',f,148+(code-first)*6);widths[code]=width
    for x in range(width):
        for y in range(height):
            if f[off+(x//8)*height+y] & (0x80>>(x%8)):
                atlas.putpixel(((code%16)*24+x,(code//16)*16+y),(255,255,255))
atlas.save(root/'assets/winfont.bmp');(root/'assets/winfont-widths.bin').write_bytes(widths)
print('Extracted MS Sans Serif 8pt:',height,'pixels high')
# Evidence preview, with the same drawing origin as the original screen.
preview=Image.new('RGB',(792,566),(0,128,128))
for x,y,s in [(20,19,'Player Takes (0)'),(360,56,'Computer Hand'),(360,416,'Player Hand'),(162,416,'Stock 40'),(687,19,'Comp Takes (0)')]:
    for ch in s:
        n=ord(ch);glyph=atlas.crop((n%16*24,n//16*16,n%16*24+widths[n],n//16*16+16));preview.paste((255,255,255),(x,y,x+widths[n],y+16),glyph.convert('L'));x+=widths[n]
preview.save(root/'build/font-reference.png')
