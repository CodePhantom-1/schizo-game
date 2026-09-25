# Kernel bug review — 2026-09-25

Scope: `kernel/` (Actions, CApi, Crafting, Db, Dialogue, Economy, Events, Faction,
Justice, Needs, Population, Property, Quests, Snapshot, Time, World), reviewed against
`kernel/contracts/*.md`. Magic.* and Schedule.* were out of scope for edits (owned by
K-1/K-2); findings there are listed at the end for their owners.

Rule followed: every fix below first landed as a test that failed on the unfixed code,
then passed after the fix. Suspicions that could not be demonstrated were dropped.
File:line references point to the pre-fix tree (`73fdc53`).

## Fixed

| # | Where (pre-fix) | Failure scenario | Fix | Test |
|---|---|---|---|---|
| 0 | `kernel/tests/test_capi.cpp:165` | The save path was hardcoded to `/tmp/claude-1000/…`. On any machine without that directory `sim_world_save` returns -2 and `test_capi` fails. | Use `std::filesystem::temp_directory_path()` and remove the file afterward. | `test_save_load_continue_through_c` |
| 1 | `kernel/src/Db.cpp:66` | A quoted field that spans lines (`"first line\r\nsecond line"`) lost its line break and parsed as `first linesecond line`. RFC 4180, and the Python `csv` module that `canon_lint.py` uses, keep the break. | Append `'\n'` to the open field when a line ends inside quotes. | `test_db: test_quoted_newline_is_kept` |
| 2 | `kernel/src/Db.cpp:100` | `canon_lint.py` applies `.strip()` to `id` and `tag`, so a row tagged `" OPEN "` passes the lint as OPEN. The kernel compared the raw text (`== "OPEN"`) in Events, Economy, World seasons, Population, Crafting and Magic, so that OPEN row would ship. A padded id (`" e2 "`) could never be found either. | `Db::load` strips `id` and `tag` the way the lint does. (No current canon row is padded; this is a latent bug.) | `test_db: test_padded_id_and_tag_are_trimmed` |
| 3 | `kernel/src/Actions.cpp:52` | `hold_hearing` correctly ignores an OPEN `laws.csv` row and falls back to compensation. When compensation went unpaid, `escalate_verdict` then read that same OPEN row's `penalty_options` and sealed `death`. That resolves an OPEN row, which the contract forbids. | Skip OPEN rows in `escalate_verdict` (same prefix test as Justice.cpp), so the verdict falls to `debt_service`. | `test_actions: test_escalation_never_reads_an_open_law_row` |
| 4 | `kernel/src/Needs.cpp:81-89` | `hours * rate` was computed in `int`. For large `hours` (for example `INT_MAX` passed from `sim_world_advance_needs`) this is signed-overflow UB. In practice the value wrapped negative, so hunger stayed at 0 and sleep *raised* fatigue. | Compute in `double` and bound the result to ±200 before the cast (any delta over 100 saturates a 0..100 meter). Results for normal inputs are unchanged. | `test_needs: test_huge_hours_saturate_not_wrap` |
| 5 | `kernel/src/Faction.cpp:39` | `s + delta` overflowed `int`: `sim_world_add_standing(w, f, INT_MAX)` from standing 50 gave **0**, not 100. | Add in `long long`, then clamp. | `test_faction: test_add_standing_extreme_delta_clamps` |
| 6 | `kernel/src/CApi.cpp:227` | `sim_world_give_item`: `count + qty` overflowed `int`. Giving `INT_MAX` and then 1 wrapped negative, and the clamp then set the stack to 0. | Add in `long long` and saturate to `[0, INT_MAX]`. | `test_capi: test_give_item_saturates` |
| 7 | `kernel/src/CApi.cpp:176,189,244` | A refused `sim_world_eat`, `sim_world_drink` or `sim_world_craft` for an actor the world has never seen ran `inventories[actor]`, which inserts an empty inventory. The call reported failure but still changed the save bytes. That breaks the Crafting contract's "untouched inv on any failure" at the boundary. | Look the inventory up without inserting it. Craft on a copy and write it back only on success. | `test_capi: test_refused_calls_leave_the_save_untouched` |
| 8 | `kernel/src/Snapshot.cpp:382` | The NPC memory count comes from the save file and went straight into `vector::reserve`. A corrupt or hostile save (for example through `sim_world_load_from_buffer`) with count 1e15 requested petabytes. Under ASan the process aborted (`allocation-size-too-big`) instead of throwing the documented `std::runtime_error`. | Remove the `reserve`. A bad count now fails as "unexpected end of data". | `test_snapshot: test_huge_section_count_throws_cleanly` (fails only under the ASan build) |
| 9 | `kernel/src/CApi.cpp` (every entry point) | C++ exceptions could cross the `extern "C"` boundary into Unreal. For example, a `std::bad_alloc` from building an `Id` or from `save_world` inside `sim_world_price`, `sim_world_eat`, `sim_world_save_to_buffer`, `sim_world_advance_days` and others would call `std::terminate` in the engine. Only `create`/`load` guarded `init`/`load_world`, and `sim_world_load`'s file read sat outside its try. | Wrap every entry point except `sim_world_destroy` (`delete` does not throw) in `try`/`catch (...)`. Each returns its documented error value: -1, nullptr, or a no-op for void functions. `sim_world_eat`/`sim_world_drink` now pass a failed lookup through as -1 rather than reporting it as -2 ("none held"). `CApi.h` documents the no-throw guarantee. | `test_capi_nothrow: test_no_exception_escapes_the_c_boundary` (new binary; replaces `operator new` with one that can be made to fail, then calls every allocating entry point) |
| 10 | `kernel/src/CApi.cpp:35,116,135` | `sim_world_create`/`sim_world_load`/`sim_world_load_from_buffer` with a wrong or missing canon directory returned a valid but empty world, because `Db::load` skips absent tables. `SimWorldSubsystem` logs "canon missing?" only on nullptr, so that check never fired. | Return nullptr when `canon_dir` holds no `seasons.csv` (`WorldState::init` builds the calendar from it). Valid canon paths behave exactly as before, and every test and the UE staging dir (`unreal/Content/Sim/canon/seasons.csv`) have it. `WorldState::init` itself is unchanged, so fixture-dir tests still work. | `test_capi: test_missing_canon_dir_is_refused` |

## Reviewed, nothing demonstrable

- **Snapshot:** every field of every state struct is written and read back, including the wave-2 fields (quest journal, outlawry, crime stage, hearing day, verdict tablet).
- **World and Context:** tick order matches `Context.hpp`. `hold_due_hearings` runs after the module ticks on the same day.
- **RNG:** only Events and Magic draw, and both use `fork`, which does not advance the parent.
- **Economy, Property, Quests, Population, Dialogue:** no unordered containers or float state found. The day math is consistent with the documented "due/deadline day itself is still live" rule.
- **Quests double-accept:** `accept()` while a quest is already active creates a second active entry. The re-accept behaviour is deliberate (`test_journal_survives_across_re_accept…`), so I did not change it.

## For other owners (not edited)

**K-1 (Magic)**

- `Magic.cpp:197`: `add_favour` computes `current += delta` in `int`. `sim_world_add_favour(w, "inanna", INT_MAX)` from 50 returns **0** (confirmed with a probe). Same fix as finding 5.
- `Magic.cpp:82`: `kPurityRequirement = 0` ignores the `rites.csv` `purity_required` column, which now exists. It is latent because every row is currently blank, but "a rite fails when performed impure" can never happen through canon.
- `Magic.cpp:236`: the roll is `fork(day)` with no rite salt, so every rite performed on the same day gets the same roll. Retrying a failed rite that day always fails, and different rites succeed or fail together.
- `Magic.cpp:227-236`: the score is a sum of doubles compared against `unit()`. If a toolchain contracts it into FMA (ARM64/Apple builds of the UE plugin), the result can differ from x86 and break cross-platform determinism. Consider an integer score (per-mille).

**K-2 (Time festivals / Schedule)**

- `Time.cpp:66`: `is_festival` uses `binary_search`, but the `Calendar` constructor sorts `seasons` (line 20), not `festival_days`. With `festival_days = {200, 10}`, `is_festival(10)` returns false (confirmed with a probe). Sort the list in the constructor.
- `Schedule.cpp:67`: `std::stoi` accepts partial input and does no range check. A `hour` of `6am` is read as 6, and `30` becomes a plan row at hour 30 (confirmed with a probe). Parse strictly and reject anything outside 0..23.

## Gate

- `cmake -S kernel -B build && cmake --build build -j4 && ctest --test-dir build --output-on-failure`: 26/26 passed.
- `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g"` in `build-asan`, full ctest with `UBSAN_OPTIONS=halt_on_error=1`: 26/26 passed, no sanitizer reports. (`test_capi_nothrow` sets `alloc_dealloc_mismatch=0` for itself only, because its replacement `operator new` otherwise trips ASan's interposed allocator.)
- `python3 tools/canon_lint.py`: LINT PASSED.
- `python3 tools/coverage_check.py`: COVERAGE PASSED.

## Follow-up

A second pass after the K-1 (rites) and K-2 (festivals, per-person schedules) merges, plus
designer decision D-022, which a coordinator message added to the scope mid-pass. Same rule:
a test that fails on the unfixed code, then the fix. Where the bug had already been fixed by
the K-2 merge, the test was checked the other way: the old code was put back, the test failed,
and the fix was restored.

| # | Where | Failure scenario | Fix | Test |
|---|---|---|---|---|
| F1 | `kernel/src/CApi.cpp` (K-1/K-2 block) | The 24 rite, festival and people entry points were merged after finding 9 and had no guard. With allocation failing, `sim_world_knows_rite` (building an `Id`) let `std::bad_alloc` through, and the test binary aborted with `terminate`. `sim_world_market_open` on a festival day also allocates (the festivals.csv lookup). | Every one of them, and `sim_world_destroy`, now uses the file's `try`/`catch (...)` pattern and returns its documented value (-1, or a no-op for the setters). `CApi.h` now documents `sim_world_omen_count`'s -1 and `sim_world_destroy` (null is a no-op; never throws), and notes that `sim_world_perform_rite`'s -1 from an internal failure may leave the rite partly applied. | `test_capi_nothrow: test_k1_k2_entry_points_do_not_throw` |
| F2 | `kernel/src/Magic.cpp` `add_favour` | `current += delta` in `int`: 50 + `INT_MAX` wrapped negative and clamped to **0**. | Add in `long long`, then clamp to 0..100 (as in finding 5). | `test_magic: test_add_favour_extreme_delta_saturates` |
| F3 | `kernel/src/Magic.cpp` purity term | `kPurityRequirement = 0` ignored `rites.csv` `purity_required`. A rite that requires purity 60 still gave the 0.20 to a performer at 59. | Read the column. Blank means the default 0. A cell that is not a plain non-negative integer also counts as 0 (reading 8 in Magic.cpp). No canon row sets a value yet, so canon outcomes do not change. | `test_magic: test_purity_required_column_gates_the_purity_term` |
| F4 | `kernel/src/Time.cpp` `is_festival` | {200, 10} in `festival_days`, then `is_festival(10)`. | **Already fixed by K-2:** the constructor sorts `festival_days` (Time.cpp:24). Duplicates are harmless to `binary_search`, so there is nothing to dedupe. A regression test was added. With the sort removed, it fails at `is_festival(10)`. | `test_time: test_unsorted_festival_days` |
| F5 | `kernel/src/Schedule.cpp` hour parsing | "6am" was read as 6, and 30 was accepted. | **Already fixed by K-2:** `parse_hour` rejects trailing text, and `build_plan` skips hours outside 0..23. A regression test was added in its own file, `test_schedule_hours.cpp`, to stay clear of concurrent `test_schedule.cpp` edits. It covers 6am, 30, 24, -1, 6.5, blank, noon, 0x6, an out-of-int value, and bad person-level rows. With the old lax parse put back, it fails. | `test_schedule_hours` (2 functions) |
| F6 | `kernel/src/Magic.cpp` roll (D-022 a) | Every rite on the same day drew `fork(day).unit()`, the same number. With seed 1 on day 5, two rites that each score 8000 bp both succeeded, and the second could never fail independently. | `ctx.rng.fork(day).fork(stable_hash(rite_id))`. `stable_hash` is FNV-1a 64, new in Rng.hpp. The same rite retried on the same day still gets the same roll. | `test_magic: test_d022_each_rite_gets_its_own_roll` (b succeeded on the old roll) |
| F7 | `kernel/src/Magic.cpp` score (D-022 b) | The score was a sum of doubles compared with `unit()`, so FMA contraction could change outcomes across CPUs. | Integer basis points: `favour*4000/100` + 2000/2000/1000/1000. Success is `roll < score_bp`, with `roll = below(10000)`. `Rng::below` is a new unbiased bounded draw. `int_in` has modulo bias, but Events depends on it, so it was left alone. `RiteResult` gains `score_bp`, and `score` is now `score_bp / 10000.0`. | `test_magic: test_d022_score_is_exact_basis_points`, `test_d022_bounded_draw`; `test_scenario_rite: test_d022_rolls_survive_save_load` |

**Re-derived fixtures (D-022).** The new rolls were probed with a throwaway program and are
listed in the header of `test_magic.cpp`. Seed 1, day 5 still gives success for every canon rite
that previously relied on it (sacrifice 3493, hymns 4837, zisurru 5312, all under their
scores). Seed 42 no longer fails the weakened sacrifice (roll 2131), so the failure fixtures
moved to seed 14 (roll 9589, which fails at both 5000 and 3000 bp). No assertion was
weakened. `test_full_formula_and_draw` now also asserts the exact roll (5807) and the success.

**Noted, not changed.** `World.cpp` reads festival `day_of_year` and season starts with lax
`strtol` ("10x" is read as 10). The `Calendar` constructor still range-checks festival days.
Snapshot casts a saved `MAGIC_PURITY` or favour to `int` without a range check. Favour is
clamped where the score reads it, so a hand-edited save cannot overflow the multiply.
`Rites.cpp`'s omen truth roll (`unit() < 0.75`) is exact in binary floating point, so it is
the same on every CPU.

### Gate (follow-up)

- `cmake -S kernel -B build && cmake --build build -j4`, `ctest`: 28/28 passed.
- ASan+UBSan (`build-asan`, `UBSAN_OPTIONS=halt_on_error=1`): 28/28 passed, no reports.
- `python3 tools/canon_lint.py`: LINT PASSED. `python3 tools/coverage_check.py`: COVERAGE PASSED.
