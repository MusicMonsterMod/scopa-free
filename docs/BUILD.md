# Build and portability

[Back to the illustrated README](../README.md). All paths below are relative to
the project root.

## Run (after a local build)

Published players should use the AppImage from the repository release. After
building here:

```sh
./dist/Scopa-Free-x86_64.AppImage
```

Click the stock pile to deal. `--seed NUMBER` selects a repeatable match,
`--dealt` skips the initial click, and `--help` lists diagnostic options.

The AppImage does not install anything. `build/ScopaFree.AppDir/AppRun` is the
same payload before it is packed.

## Rebuild

```sh
./build.sh
```

Prerequisites: x86-64 Linux, G++ with C++17, SDL2 development headers, pkg-config,
Python 3, squashfs-tools (`mksquashfs`), ldd and standard shell utilities. No
package manager is invoked and no dependencies are installed automatically.

Normal builds are offline. The official type-2 AppImage runtime is pinned by
`vendor/SHA256SUMS`, version 8f39b89. The script compiles `src/tests.cpp`, runs
500 complete simulated matches and targeted rule tests, compiles the UI,
assembles an AppDir, checks it using SDL's dummy driver, then creates a
SquashFS-backed type-2 AppImage.

If `assets/icons/scopa-icon.png` changes, the build regenerates the icon copies
using Pillow. Otherwise its pre-generated copies are sufficient and Pillow is not
a build requirement. `tools/prepare_icon.py` reads that PNG, creates an
SDL-readable BMP and generates standard desktop icon sizes with Lanczos
resampling.

## Optional development checks

```sh
python3 tools/compare_reference.py
./tools/ui-smoke.sh
./tools/ui-round-test.sh
```

The pixel comparison requires Pillow. The two interaction scripts require Xvfb,
xdotool and ImageMagick. They place their output in `build/`. The round script
records the score dialog and subsequent round for visual inspection.

`build/VALIDATION.txt` records the checks completed for the delivered build.
The pixel comparison covers the original idle client area; it does not claim
that the independent engine reproduces every original game or dialog pixel.

## Asset provenance and regeneration

- `tools/extract_assets.py`: extracts bitmap and icon resources from the supplied
  PE files without executing them. Requires Pillow; normal builds use included
  assets instead. This preserves the supplied Linux icon.
- `tools/extract_bitmap_font.py`: extracts the 8pt MS Sans Serif bitmap strike
  from Wine's `sserife.fon`. Only this optional regeneration step needs that file;
  running or building the game does not need Wine.
- `assets/menu-bar.bmp`: the original fixed Game/Help menu strip, captured from
  the reference executable. Menu hit regions and dropdowns are native code.
- `tools/make_readme_art.py`: composes README graphics from the original bitmaps
  at the game's native 792px width using nearest-neighbor integer scaling, so the
  bitmap font is not browser-scaled. Requires Pillow.

The source cards remain exactly 71×125 pixels. Selected cards invert RGB values.
The 792×566 layout is the baseline. Cards never stretch. A larger window keeps
71×125 card bitmaps and widens the gaps between piles, hands and table slots,
like classic Solitaire; the menu strip continues across the extra width. Native
window decorations are provided by your desktop. The original window caption
is deliberately retained even though the executable is Linux-native.

## Portability boundary

The AppImage contains an x86-64 Linux executable, SDL2, recursively discovered
shared libraries, and their matching GNU libc loader. `AppRun` invokes the
bundled loader with a private library path, avoiding a dependency on the host's
libc version. It does not export a library path into unrelated child processes.

Rendering is software-only. X11 is verified; the bundled SDL2 also supports
Wayland, with libdecor plugins disabled to avoid loading host toolkit plugins.
A working desktop display server is still required. Audio depends on usable host
sound services/devices and becomes silent if no device can be opened.

This is not an ARM build or a universal Linux compatibility guarantee. It was
validated on Linux Mint/X11; CachyOS and live Wayland were not tested. The bundled
GNU libc targets Linux kernel 3.2 or newer, but other host services may impose
newer requirements.

If mounting the AppImage is unavailable, run:

```sh
./dist/Scopa-Free-x86_64.AppImage --appimage-extract-and-run
```

That runtime uses a temporary directory and cleans it up afterward.

The game itself stores no persistent preferences, statistics or saves. Nothing
is registered in application menus or copied into the home directory.

## Moving the folder

The AppImage is relocatable. Copy `dist/Scopa-Free-x86_64.AppImage` and run it
from anywhere.

## Differences from the Windows engine

This is a native recreation from artwork and observed behavior, not a source
port or binary translation. The computer strategy and seeded shuffling are new.
Seeds are not compatible with Windows deals and may depend on the C++ standard
library implementation. Statistics last for the current session only.

The original option labels are implemented using these variant definitions:
Scopa D'Assi takes the table with an unmatched ace without awarding a sweep;
Napoli scores a run of at least three coins starting at the ace; Re Bello scores
the king of coins; Cappotto replaces ordinary round points with the configured
amount when one player collects all 40 cards. These definitions are documented
in Help and are not a claim of reverse-engineered equivalence for every variant.
