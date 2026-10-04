# Visual reference

The supplied `original/ScopaFree.exe` was run only as a reference, using an
isolated Wine prefix inside `build/`. It is never used by the native executable,
launcher or AppImage. No Windows program is shipped inside the AppImage.

The captured initial client area is `tests/reference/original-idle.png`:
792×566 pixels, including the original Game/Help menu strip. At native size,
`python3 tools/compare_reference.py` compares the native output to that capture.
The delivered idle screen has **zero differing pixels out of 448,272**.

This verifies the teal background, original bitmap cards and placeholders,
stock and capture pile positions, labels, and default menu strip. It excludes
window-manager decorations, and does not imply whole-program equivalence.

The 71×125 card bitmaps and empty markers are extracted directly from
`scopadll.dll`. The 8pt MS Sans Serif strike matches the original table labels
without anti-aliasing. The fixed normal menu strip is a reference bitmap; its
hit regions and dropdown behavior are native. Dialogs use native drawn controls
and positions from the original dialog resources; their decoration can differ
from Windows/Wine. Your supplied denari icon intentionally replaces the old icon.

The font and original card resources retain their respective notices. See
`NOTICE.md` and the packaged license directory for provenance.
