#!/usr/bin/env python3
"""Extract original Windows resources without executing either PE file."""
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import io
ROOT = Path(__file__).resolve().parent.parent

def resources(path):
    data = path.read_bytes()
    u16 = lambda off: struct.unpack_from('<H', data, off)[0]
    u32 = lambda off: struct.unpack_from('<I', data, off)[0]
    pe = u32(60)
    sections = pe + 24 + u16(pe + 20)
    def offset(rva):
        for i in range(u16(pe + 6)):
            s = sections + i * 40
            size, va, rawsize, raw = struct.unpack_from('<IIII', data, s + 8)
            if va <= rva < va + max(size, rawsize):
                return raw + rva - va
        raise ValueError(f'Invalid RVA {rva:x}')
    base = offset(u32(pe + 24 + 112))
    def walk(rel, parts=()):
        here = base + rel
        for i in range(u16(here + 12) + u16(here + 14)):
            name, child = struct.unpack_from('<II', data, here + 16 + i * 8)
            if name & 0x80000000:
                p = base + (name & 0x7fffffff)
                name = data[p+2:p+2+u16(p)*2].decode('utf-16le')
            if child & 0x80000000:
                yield from walk(child & 0x7fffffff, parts + (name,))
            else:
                rva, size = struct.unpack_from('<II', data, base + child)
                yield parts + (name,), data[offset(rva):offset(rva)+size]
    return dict(walk(0))

def main():
    out = ROOT / 'assets'
    out.mkdir(exist_ok=True)
    for (kind, name, lang), dib in resources(ROOT/'original/scopadll.dll').items():
        if kind != 2: continue
        bits = struct.unpack_from('<H', dib, 14)[0]
        colors = struct.unpack_from('<I', dib, 32)[0] or (1 << bits if bits <= 8 else 0)
        header = struct.unpack_from('<I', dib)[0]
        bmp = b'BM' + struct.pack('<IHHI', len(dib)+14, 0, 0, 14+header+colors*4) + dib
        Image.open(io.BytesIO(bmp)).convert('RGB').save(out / (str(name).lower()+'.bmp'))
    exe = resources(ROOT/'original/ScopaFree.exe')
    group = next(v for k,v in exe.items() if k[0] == 14)
    count = struct.unpack_from('<H', group, 4)[0]
    entries, payload = [], b''
    for i in range(count):
        rec = group[6+i*14:20+i*14]
        rid = struct.unpack_from('<H', rec, 12)[0]
        raw = next(v for k,v in exe.items() if k[0] == 3 and k[1] == rid)
        entries.append(rec[:8]+struct.pack('<II',len(raw),6+16*count+len(payload)))
        payload += raw
    ico = struct.pack('<HHH',0,1,count)+b''.join(entries)+payload
    (out/'original.ico').write_bytes(ico)
    icon = Image.open(io.BytesIO(ico)).convert('RGBA')
    icon.save(out/'original-icon.png')
    # SDL's BMP loader handles the opaque window icon.
    icon.convert('RGB').save(out/'original-icon.bmp')
    print('Extracted',len(list(out.glob('*.bmp'))),'bitmaps and original icon')
if __name__ == '__main__': main()
