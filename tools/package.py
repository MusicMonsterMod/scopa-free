#!/usr/bin/env python3
"""Assemble a self-contained AppDir, including its matching GNU loader."""
import os, re, shutil, subprocess
from pathlib import Path
root=Path(__file__).resolve().parent.parent
app=root/'build/ScopaFree.AppDir'
if app.exists(): shutil.rmtree(app)
for folder in ['usr/bin','usr/lib','usr/share/scopa-free','usr/share/licenses','usr/share/applications','usr/share/icons/hicolor/256x256/apps']:
    (app/folder).mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'build/scopa-free',app/'usr/bin/scopa-free')
for path in (root/'assets').iterdir():
    if path.suffix in {'.bmp','.wav','.png','.svg','.ico','.bin'}:shutil.copy2(path,app/'usr/share/scopa-free'/path.name)
# ldd is used only on the ELF we just compiled, never the supplied Windows files.
output=subprocess.check_output(['ldd',str(root/'build/scopa-free')],text=True)
if 'not found' in output:raise SystemExit(output)
libs=set(re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)',output,re.M))
packages=set()
for lib in sorted(libs):
    path=Path(lib)
    shutil.copy2(path.resolve(),app/'usr/lib'/path.name)
    for candidate in [path,path.resolve(),Path(str(path).replace('/lib/','/usr/lib/',1))]:
        p=subprocess.run(['dpkg-query','-S',str(candidate)],capture_output=True,text=True) if shutil.which('dpkg-query') else None
        if p and p.returncode==0:
            packages.update(line.split(': ')[0].split(':')[0] for line in p.stdout.splitlines());break
licenses=app/'usr/share/licenses'
for package in sorted(packages):
    copyright=Path('/usr/share/doc')/package/'copyright'
    if copyright.exists():shutil.copy2(copyright,licenses/(package+'.txt'))
for p in Path('/usr/share/common-licenses').glob('*'):
    if p.is_file():shutil.copy2(p,licenses/p.name)
fontlicense=root/'vendor/wine-font-LICENSE.txt'
if fontlicense.exists():shutil.copy2(fontlicense,licenses/'Wine-fonts.txt')
for name in ['README.md','NOTICE.md']:
    if (root/name).exists():shutil.copy2(root/name,licenses/name)
shutil.copy2(root/'vendor/runtime-LICENSE.txt',licenses/'AppImage-runtime.txt')
(app/'usr/share/licenses/bundled-libraries.txt').write_text('\n'.join(sorted(libs))+'\n')
# AppImage root / .DirIcon: prefer high-quality 256 (prepare_icon writes scopa-free.png as 256).
root_icon=root/'assets/icons/256.png'
if not root_icon.exists():
    root_icon=root/'assets/scopa-free.png'
shutil.copy2(root_icon,app/'scopa-free.png')
for size in [16,32,48,64,128,256,512]:
    icon_dir=app/f'usr/share/icons/hicolor/{size}x{size}/apps'
    icon_dir.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/f'assets/icons/{size}.png',icon_dir/'scopa-free.png')
(app/'.DirIcon').symlink_to('scopa-free.png')
desktop='''[Desktop Entry]
Type=Application
Name=Scopa Free
Comment=The Italian card game of taking cards
Exec=scopa-free
Icon=scopa-free
Terminal=false
Categories=Game;CardGame;
StartupWMClass=scopa-free
'''
(app/'scopa-free.desktop').write_text(desktop)
(app/'usr/share/applications/scopa-free.desktop').write_text(desktop)
(app/'AppRun').write_text('''#!/bin/sh
set -eu
APPDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# Keep bundled libc paired with its loader. Do not inject libraries into child programs.
# Software rendering avoids depending on host GPU driver ABI versions.
export SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR=0
exec "$APPDIR/usr/lib/ld-linux-x86-64.so.2" --library-path "$APPDIR/usr/lib" "$APPDIR/usr/bin/scopa-free" "$@"
''')
(app/'AppRun').chmod(0o755)
print('Bundled',len(libs),'shared libraries with their loader in',app)
