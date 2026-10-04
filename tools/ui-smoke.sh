#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
Xvfb :97 -screen 0 1800x1250x24 -nolisten tcp >build/xvfb.log 2>&1 &
xvfb=$!
app=''
trap '[[ -z "$app" ]] || kill "$app" 2>/dev/null || true; kill "$xvfb" 2>/dev/null || true' EXIT
export DISPLAY=:97 SDL_AUDIODRIVER=dummy
sleep 0.4
build/scopa-free --seed 42 >build/ui-smoke.log 2>&1 &
app=$!
sleep 0.5
win=$(xdotool search --onlyvisible --pid "$app" | head -1)
xdotool mousemove --window "$win" 180 480 click 1
sleep 0.2
import -window "$win" build/ui-classic-dealt.png
xdotool key --window "$win" h
sleep 0.2
import -window "$win" build/ui-classic-hint.png
xdotool key --window "$win" Return
sleep 0.9
import -window "$win" build/ui-classic-after-turn.png
xdotool key --window "$win" F5
sleep 0.2
import -window "$win" build/ui-classic-options.png
xdotool key --window "$win" Escape
xdotool key --window "$win" F3
sleep 0.1
xdotool key --window "$win" ctrl+a
xdotool type --window "$win" 1234
xdotool key --window "$win" Return
sleep 0.2
xdotool getwindowname "$win" >build/ui-seed-title.txt
rg '#1234' build/ui-seed-title.txt
xdotool key --window "$win" space
sleep 0.2
xdotool mousemove --window "$win" 18 8 click 1
sleep 0.2
import -window "$win" build/ui-classic-menu.png
xdotool key --window "$win" Escape
xdotool windowmove "$win" 0 0
xdotool windowsize "$win" 1584 1132
sleep 0.2
import -window "$win" build/ui-classic-2x.png
kill -0 "$app"
printf '%s\n' 'PASS: classic mouse deal, selection, play, AI, options, seed dialog, menu, integer resizing'
