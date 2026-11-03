#!/bin/sh
# Capture real X terminals replaying the saved guest/grader logs, not a live guest.
set -eu
cd "$(dirname "$0")/.."

if [ -z "${DISPLAY:-}" ]; then
    Xvfb :99 -screen 0 1280x960x24 -nolisten tcp &
    server=$!
    trap 'kill "$server" 2>/dev/null || true' EXIT
    export DISPLAY=:99
    sleep 1
    sh scripts/capture_lab3.sh
    exit
fi

capture() {
    name=$1
    command=$2
    xterm -title "Lab3 $name" -fa 'DejaVu Sans Mono' -fs 11 \
        -geometry 112x42+0+0 -bg '#101820' -fg '#e8edf2' \
        -e sh -c "$command; sleep 60" &
    terminal=$!
    sleep 1
    window=$(xwininfo -name "Lab3 $name" | awk '/Window id:/ {print $4; exit}')
    magick import -window "$window" "report/images/$name.png"
    kill "$terminal"
    wait "$terminal" 2>/dev/null || true
}

mkdir -p report/images
capture boot 'printf "%s\n\n" "$ tail -n 32 report/evidence/boot.log"; tail -n 32 report/evidence/boot.log'
capture grade 'printf "%s\n\n" "Saved course grader output (filtered from report/evidence/grade.log)"; grep -E "injection complete|Building|Running|PASS|FAIL|Total Score" report/evidence/grade.log'
capture exceptions 'printf "%s\n\n" "$ tail -n 31 report/evidence/probe.log"; tail -n 31 report/evidence/probe.log'
