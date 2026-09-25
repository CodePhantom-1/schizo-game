#!/usr/bin/env bash
# ue_smoke.sh — A3: boots the real game headless and proves the chain lives:
# the map loads, the city stands, the kernel world is created from the canon,
# a whole day turns over through the console, and nothing fatal is logged.
# Usage: tools/ue_smoke.sh   (UE_ROOT defaults to ~/UnrealEngine). Exit 1 on any miss.
set -uo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine}"
LOG="$(mktemp -t ue_smoke.XXXXXX.log)"
timeout 600 "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" "$REPO/unreal/SchizoGame.uproject" -game -nullrhi -unattended \
	-ExecCmds="sim.Dump, sim.AdvanceDays 1, sim.AdvanceHours 12, sim.Dump, quit" -log > "$LOG" 2>&1
RC=$?
TEXT="$(tr -d '\0' < "$LOG")"
fail=0
need() { grep -qaE "$1" <<<"$TEXT" && echo "ok   $2" || { echo "MISS $2"; fail=1; }; }
never() { grep -qaE "$1" <<<"$TEXT" && { echo "BAD  $2"; fail=1; } || echo "ok   $2"; }
[[ $RC -eq 0 ]] && echo "ok   the game quit cleanly" || { echo "BAD  the game exited rc=$RC"; fail=1; }
need 'The City of the Moon stands' "the city was built"
need 'Sim world created from .*: day 1' "the kernel world was created from the canon"
need 'sim: day 1 \(1 Rains-Coming, year 1\)' "day 1 with its month name"
need 'sim: day 2 \(2 Rains-Coming, year 1\) 1[78]\.[0-9]+h' "a day and a half later: day 2, afternoon"
never 'Fatal error|Assertion failed|Ensure condition failed|=== Handled ensure' "no crash, assert or ensure"
echo "log: $LOG"
exit $fail
