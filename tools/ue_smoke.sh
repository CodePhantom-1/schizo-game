#!/usr/bin/env bash
# ue_smoke.sh — A3: boots the real game headless and proves the chain lives:
# the map loads, the city stands, the kernel world is created from the canon,
# a whole day turns over through the console, and nothing fatal is logged.
# Usage: tools/ue_smoke.sh   (UE_ROOT defaults to ~/UnrealEngine). Exit 1 on any miss.
set -uo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine}"
# Linux build, or a Windows install run from Git Bash (Tommy's box).
if [[ -e "$UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ]]; then UE_BIN="$UE_ROOT/Engine/Binaries/Win64"; UE_EXE=.exe; UE_STDOUT="-stdout -FullStdOutLogOutput"; else UE_BIN="$UE_ROOT/Engine/Binaries/Linux"; UE_EXE=; UE_STDOUT=; fi
LOG="$(mktemp -t ue_smoke.XXXXXX.log)"
timeout 600 "$UE_BIN/UnrealEditor$UE_EXE" "$REPO/unreal/SchizoGame.uproject" -game -nullrhi -unattended $UE_STDOUT -SimNoMenu \
	-ExecCmds="sim.Dump, sim.Accept the_outsiders_first_days, sim.Quests active, sim.Talk the captain of the prisoner transport, sim.AdvanceDays 1, sim.AdvanceHours 12, sim.Dump, sim.SaveSlot smoke_a Smoke test, sim.AdvanceDays 3, sim.LoadSlot smoke_a, sim.Dump, sim.AdvanceDays 5, sim.NewGame, sim.Dump, sim.LoadSlot no_such_slot, sim.DeleteSlot smoke_a, quit" -log > "$LOG" 2>&1
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
need 'sim.Accept the_outsiders_first_days: accepted' "a quest can be accepted"
need 'the_outsiders_first_days — The Outsider.s First Days' "the active quest shows its title"
need "sim.Talk 'the captain of the prisoner transport': [1-9]" "the captain has lines to say"
need 'Saved slot smoke_a' "a slot was saved"
need 'Loaded slot smoke_a' "and loaded back"
# The dump after the load must match the dump before the save (same day, same hour).
DUMPS="$(grep -aoE 'sim: day [0-9]+ \([^)]*\) [0-9.]+h' <<<"$TEXT")"
BEFORE="$(sed -n 2p <<<"$DUMPS")"; AFTER="$(sed -n 3p <<<"$DUMPS")"
[[ -n "$BEFORE" && "$BEFORE" == "$AFTER" ]] && echo "ok   the load restored the day and hour ($AFTER)" || { echo "BAD  after the load: '$AFTER', saved at: '$BEFORE'"; fail=1; }
LAST="$(tail -1 <<<"$DUMPS")"
[[ "$LAST" == "sim: day 1 (1 Rains-Coming, year 1)"* ]] && echo "ok   New Game starts a fresh world on day 1" || { echo "BAD  after New Game: '$LAST'"; fail=1; }
need 'Shell: toast There is no such save' "a failed load tells the player"
never 'Fatal error|Assertion failed|Ensure condition failed|=== Handled ensure' "no crash, assert or ensure"
# A plain boot (no -SimNoMenu) shows the loading notice, then the main menu over the city.
LOG2="$(mktemp -t ue_smoke_menu.XXXXXX.log)"
# No quit: it runs until the menu has had time to appear, then the timeout ends it (rc 124).
timeout 120 "$UE_BIN/UnrealEditor$UE_EXE" "$REPO/unreal/SchizoGame.uproject" -game -nullrhi -unattended $UE_STDOUT \
	-log > "$LOG2" 2>&1
TEXT2="$(tr -d '\0' < "$LOG2")"
grep -qa 'Shell: open Loading' <<<"$TEXT2" && echo "ok   the loading notice opens on boot" || { echo "MISS the loading notice on boot"; fail=1; }
grep -qa 'Shell: open MainMenu' <<<"$TEXT2" && echo "ok   then the main menu, over the city" || { echo "MISS the main menu after loading"; fail=1; }
grep -qaE 'Fatal error|Assertion failed|Ensure condition failed' <<<"$TEXT2" && { echo "BAD  the menu boot logged a crash or ensure"; fail=1; } || echo "ok   the menu boot is clean"
# The smoke test leaves no save behind (it must never become the developer's "Continue").
[[ ! -f "$REPO/unreal/Saved/SaveGames/smoke_a.sav" ]] && echo "ok   the smoke save was cleaned up" || { echo "BAD  smoke_a.sav left behind"; fail=1; }
echo "log: $LOG (menu run: $LOG2)"
exit $fail
