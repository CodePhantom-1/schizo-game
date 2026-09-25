#!/usr/bin/env bash
# ue_test.sh — runs the project's Unreal automation tests headless (A3).
# Usage: tools/ue_test.sh [filter=Sim.]   (UE_ROOT defaults to ~/UnrealEngine)
# Prints one line per project test; exits 1 if any fails or none ran.
set -uo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$HOME/UnrealEngine}"
FILTER="${1:-Sim.}"  # a trailing dot is dropped for UE (it matches nothing) but kept for the path check
LOG="$(mktemp -t ue_test.XXXXXX.log)"
timeout 1800 "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$REPO/unreal/SchizoGame.uproject" \
	-nullrhi -unattended -nosplash -ExecCmds="Automation RunTests ${FILTER%.}" \
	-TestExit="Automation Test Queue Empty" -log > "$LOG" 2>&1
# The filter is a substring match, so keep only the project's own tests.
RESULTS="$(grep -E "Test Completed\. Result=\{[A-Za-z]+\} Name=\{[^}]*\} Path=\{$FILTER" "$LOG" | sed -E 's/.*Result=\{([A-Za-z]+)\}.*Path=\{([^}]*)\}.*/\1 \2/')"
echo "${RESULTS:-no tests matched $FILTER}"
echo "log: $LOG"
[[ -n "$RESULTS" ]] && ! grep -qv '^Success ' <<<"$RESULTS"
