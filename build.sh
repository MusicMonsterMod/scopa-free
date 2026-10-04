#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if [[ $(uname -m) != x86_64 ]]; then
    echo 'This AppImage recipe currently targets x86_64 Linux.' >&2; exit 1
fi
for tool in g++ pkg-config python3 mksquashfs sha256sum; do
    command -v "$tool" >/dev/null || { echo "Missing build dependency: $tool (nothing has been installed)" >&2; exit 1; }
done
pkg-config --exists sdl2 || { echo 'SDL2 development headers are required to build.' >&2; exit 1; }
mkdir -p build dist
# Always regenerate icons so cmp cannot leave stale nearest-neighbor sized PNGs / huge icon.bmp.
python3 tools/prepare_icon.py
sha256sum -c vendor/SHA256SUMS
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic src/tests.cpp -o build/tests
./build/tests
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic src/main.cpp -o build/scopa-free "${sdl_flags[@]}"
python3 tools/package.py
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/ScopaFree.AppDir/AppRun --smoke-test
python3 -c 'from pathlib import Path; Path("build/scopa.squashfs").unlink(missing_ok=True)'
mksquashfs build/ScopaFree.AppDir build/scopa.squashfs -noappend -comp gzip -all-root -no-xattrs -processors 2 -quiet
# Write via tempfile then mv to avoid ETXTBSY when the AppImage is in use.
tmp_appimage="$(mktemp dist/Scopa-Free-x86_64.AppImage.XXXXXX)"
cat vendor/runtime-x86_64 build/scopa.squashfs > "$tmp_appimage"
chmod +x "$tmp_appimage"
mv -f "$tmp_appimage" dist/Scopa-Free-x86_64.AppImage
sha256sum dist/Scopa-Free-x86_64.AppImage > dist/SHA256SUMS
printf '\nBuilt: %s/dist/Scopa-Free-x86_64.AppImage\n' "$PWD"
