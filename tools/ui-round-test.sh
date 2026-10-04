#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
Xvfb :96 -screen 0 1280x900x24 -nolisten tcp >build/xvfb-round.log 2>&1 &
xvfb=$!
app=''
trap '[[ -z "$app" ]] || kill "$app" 2>/dev/null || true; kill "$xvfb" 2>/dev/null || true' EXIT
export DISPLAY=:96 SDL_AUDIODRIVER=dummy
sleep 0.4
build/scopa-free --seed 42 >build/ui-round.log 2>&1 &
app=$!
sleep 0.5
win=$(xdotool search --onlyvisible --pid "$app" | head -1)
for i in $(seq 1 18); do
    xdotool key --window "$win" space
    sleep 0.08
    xdotool key --window "$win" h
    sleep 0.06
    xdotool key --window "$win" Return
    sleep 1.05
done
import -window "$win" build/ui-score.png
xdotool key --window "$win" Return
xdotool key --window "$win" space
sleep 1.1
import -window "$win" build/ui-next-round.png
kill -0 "$app"
printf '%s\n' 'UI round test completed; inspect ui-score.png and ui-next-round.png.'
