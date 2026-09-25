#!/usr/bin/env bash
# test_ue_test.sh — ue_test.sh's verdict on crafted logs (no editor needed).
set -u
T="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/ue_test.sh"
D="$(mktemp -d)"; fails=0
ok()   { UE_TEST_LOG="$D/$1" UE_TEST_RC="${2:-0}" "$T" Sim. >/dev/null 2>&1 && echo "pass $1" || { echo "FAIL $1 (expected success)"; fails=1; }; }
bad()  { UE_TEST_LOG="$D/$1" UE_TEST_RC="${2:-0}" "$T" Sim. >/dev/null 2>&1 && { echo "FAIL $1 (expected failure)"; fails=1; } || echo "pass $1"; }
line() { echo "LogAutomationController: Display: Test Completed. Result={$1} Name={x} Path={$2}"; }
{ echo "Found 3 automation tests based on 'Sim'"; line Success Sim.A.a; line Success Simple.B; line Success Sim.C.c; } > "$D/all_ran.log"
{ echo "Found 3 automation tests based on 'Sim'"; line Success Sim.A.a; line Success Simple.B; } > "$D/crashed_midway.log"
{ echo "Found 2 automation tests based on 'Sim'"; line Success Sim.A.a; line Fail Sim.B.b; } > "$D/one_failed.log"
{ echo "Found 1 automation tests based on 'Sim'"; line Success SimXA.a; } > "$D/no_project_test.log"
ok  all_ran.log
bad crashed_midway.log
bad one_failed.log
bad no_project_test.log
bad all_ran.log 124        # the editor timed out
bad all_ran.log 139        # the editor crashed (SIGSEGV)
ok  all_ran.log 1          # UE exits 1 when any engine test failed; ours all ran and passed
rm -rf "$D"; exit $fails
