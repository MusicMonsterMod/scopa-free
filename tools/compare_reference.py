#!/usr/bin/env python3
"""Compare the native idle client area with the actual original Win32 app."""
import os, subprocess
from pathlib import Path
from PIL import Image, ImageChops
root=Path(__file__).resolve().parent.parent
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
subprocess.run([str(root/'build/scopa-free'),'--seed','42','--smoke-test','--screenshot',str(root/'build/reference-check.bmp')],env=env,check=True)
a=Image.open(root/'tests/reference/original-idle.png').convert('RGB')
b=Image.open(root/'build/reference-check.bmp').convert('RGB')
if a.size!=b.size:raise SystemExit(f'Size mismatch: {a.size} vs {b.size}')
diff=ImageChops.difference(a,b);count=sum(p!=(0,0,0) for p in diff.getdata())
if count:
    diff.save(root/'build/reference-diff.png')
    raise SystemExit(f'FAIL: {count} pixels differ (see build/reference-diff.png)')
print(f'PASS: original idle client area matches exactly ({a.width*a.height:,} pixels)')
