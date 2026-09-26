#!/usr/bin/env bash
# ue_test.sh — runs the project's Unreal automation tests headless (A3).
# Usage: tools/ue_test.sh [filter=Sim.]   (UE_ROOT defaults to ~/UnrealEngine)
# Prints one line per project test. Exits 1 unless the editor exited cleanly,
# every test UE found completed, and every project test (path under the
# filter) succeeded — a crash or timeout midway is a failure, never a pass.
# Testing hook: UE_TEST_LOG=<log> [UE_TEST_RC=<rc>] judges an existing log
# instead of launching the editor (tools/tests/test_ue_test.sh).
set -uo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine}"
# Linux build, or a Windows install run from Git Bash (Tommy's box).
if [[ -e "$UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ]]; then UE_BIN="$UE_ROOT/Engine/Binaries/Win64"; UE_EXE=.exe; UE_STDOUT="-stdout -FullStdOutLogOutput"; else UE_BIN="$UE_ROOT/Engine/Binaries/Linux"; UE_EXE=; UE_STDOUT=; fi
FILTER="${1:-Sim.}"  # UE RunTests matches nothing for dotted filters, so it gets the first segment and the path check does the rest

if [[ -n "${UE_TEST_LOG:-}" ]]; then
	LOG="$UE_TEST_LOG"
	RC="${UE_TEST_RC:-0}"
else
	LOG="$(mktemp -t ue_test.XXXXXX.log)"
	timeout 1800 "$UE_BIN/UnrealEditor-Cmd$UE_EXE" "$REPO/unreal/SchizoGame.uproject" \
		-nullrhi -unattended -nosplash $UE_STDOUT -ExecCmds="Automation RunTests ${FILTER%%.*}" \
		-TestExit="Automation Test Queue Empty" -log > "$LOG" 2>&1
	RC=$?
fi

FOUND="$(grep -oE 'Found [0-9]+ automation tests' "$LOG" | grep -oE '[0-9]+' | tail -1)"
COMPLETED="$(grep -cE 'Test Completed\. Result=' "$LOG")"
PATH_RE="$(printf '%s' "$FILTER" | sed 's/[.[\*^$]/\\&/g')"
# UE's filter is a substring match, so keep only the project's own tests.
RESULTS="$(grep -E "Test Completed\. Result=\{[A-Za-z]+\} Name=\{[^}]*\} Path=\{$PATH_RE" "$LOG" | sed -E 's/.*Result=\{([A-Za-z]+)\}.*Path=\{([^}]*)\}.*/\1 \2/')"
echo "${RESULTS:-no tests matched $FILTER}"
echo "log: $LOG (editor rc=$RC, found=${FOUND:-?}, completed=$COMPLETED)"

# 124 = timeout, >=128 = killed by a signal (crash). UE exits 1 when ANY test
# failed, engine tests included, so 1 is judged by the counts and results.
[[ "$RC" -eq 0 || "$RC" -eq 1 ]] || { echo "editor did not exit cleanly (rc=$RC)"; exit 1; }
[[ -n "$FOUND" && "$FOUND" -eq "$COMPLETED" ]] || { echo "not every test completed"; exit 1; }
[[ -n "$RESULTS" ]] || exit 1
! grep -qv '^Success ' <<<"$RESULTS"
