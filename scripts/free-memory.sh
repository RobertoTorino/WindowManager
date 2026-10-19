#!/usr/bin/env bash
# Frees memory before testing a game on Linux (the BC-250 dev box has 9.4 GB of RAM next to 6 GB of VRAM).
#
#   scripts/free-memory.sh          list what uses memory and what would be closed
#   scripts/free-memory.sh --close  close those apps (politely: SIGTERM, they may save their state)
#
# VS Code is never closed: Claude Code runs inside it.

set -u

# Process names (as `ps -o comm` prints them, max 15 chars) of apps that are safe to close for a test.
CLOSABLE=(
    chrome brave firefox
    jetbrains-toolb idea pycharm clion rustrover rust-analyzer clippy-driver cargo rustc
    discord vesktop spotify thunderbird
    dolphin okular gwenview
    baloo_file baloorunner
)

summary() {
    echo "== Memory =="
    free -h | head -2
    swapon --show --noheadings | awk '{print "Swap (" $1 "): " $4 " of " $3 " used"}'
    echo
    echo "== Largest apps (all processes of one name added up) =="
    ps -eo rss=,comm= | awk '{m[$2]+=$1} END {for (c in m) printf "%7.0f MB  %s\n", m[c]/1024, c}' \
        | sort -rn | head -12
}

closable_running() {
    for name in "${CLOSABLE[@]}"; do
        pgrep -x "$name" >/dev/null && echo "$name"
    done
}

summary
echo
mapfile -t running < <(closable_running)
if [ "${#running[@]}" -eq 0 ]; then
    echo "Nothing to close."
    exit 0
fi

if [ "${1:-}" != "--close" ]; then
    echo "Would close: ${running[*]}"
    echo "Run with --close to close them."
    exit 0
fi

for name in "${running[@]}"; do
    echo "Closing $name"
    pkill -TERM -x "$name"
done
sleep 3
for name in "${running[@]}"; do
    pgrep -x "$name" >/dev/null && echo "Still running: $name (close it by hand)"
done
echo
summary
