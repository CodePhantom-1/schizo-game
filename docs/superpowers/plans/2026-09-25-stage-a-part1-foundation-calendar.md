# Stage A, part 1 — Foundation and the Calendar — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Tommy's city runs on Linux; CI guards every push; the calendar is turned on (dates, month names, moon phases, favourable and unfavourable days, the moon observances) from the kernel through to the HUD and the sky; the day length defaults to 48 minutes; a developer console reaches every system that is live today; and a UE automation suite proves the chain boots.

**Architecture:** The kernel (`kernel/`, C++20, headless) owns all calendar rules: `Calendar` gains a moon cycle and per-day-of-month definitions loaded from two new canon tables. The C API (`kernel/include/sim/CApi.h`) exposes them as plain C calls. `USimWorldSubsystem` (the SimRuntime plugin) wraps each C call as a Blueprint-pure getter. `ASimHud` draws them, and `ASimDayNight` places the moon by phase. Console commands live next to the systems they drive: kernel-only commands in SimRuntime, street-dependent ones in the SchizoGame module.

**Tech Stack:** C++20 + CMake + ctest (the kernel, with its zero-dependency `sim/Test.hpp` harness); Python 3 (canon tools); Unreal Engine 5.8.3 built from source at `~/UnrealEngine` (UBT, the Automation framework); GitHub Actions.

**Spec:** [docs/completion-plan.md](../../completion-plan.md), Stage A steps A1–A5, A13 and A14; decisions DQ2 and DQ7 in [DECISIONS.md](../../../DECISIONS.md) D-024.

## Global Constraints

- Every canon row carries `id`, `tag` (CANON / A / INVENTED / OPEN) and `source_ref`; `python3 tools/canon_lint.py` must print `LINT PASSED`.
- `python3 tools/coverage_check.py` must print `COVERAGE PASSED`.
- After any change under `db/canon/`, run `python3 tools/stage_canon_for_ue.py`, then `python3 tools/stage_canon_for_ue.py --check` must exit 0.
- Kernel gate: `cmake -S kernel -B /tmp/claude-1000/sim-build-main -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/claude-1000/sim-build-main -j && ctest --test-dir /tmp/claude-1000/sim-build-main --output-on-failure` all pass.
- The kernel is deterministic and integer-only for game rules (D-022): no floats in any calendar rule.
- Every C API function is `extern "C"`, never throws (it wraps its body in `try { } catch (...) { return -1; }`), returns -1 on a null world, and string writers return the full length and truncate to `cap-1`.
- UE code never caches the `SimWorld*` handle; it fetches it on each use through `USimWorldSubsystem::GetSimHandleFor(this)`.
- Player-facing UE text goes through `NSLOCTEXT("SimHud", "<Key>", "...")`/`FText::Format`, never a bare `TEXT()` literal shown on screen (localisation-ready, plan.md §9).
- The Linux launch never uses `-opengl4`, and never triggers runtime sky capture (DECISIONS standing rule).
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- After the whole plan: bug review of the merged range, fix, re-run the gate, push `main` (the designer's rule).

## Review Focus

- **A save made before this change.** `calendar_days`/`months` are loaded from the canon, not stored in the snapshot, so an old save must load and show the new month names. Pinned in Task 4, `test_calendar_survives_save_load`.
- **Canon missing a new table** (an old staged folder, a partial checkout). The calendar must degrade to unnamed months and no observances, never crash or throw. Pinned in Task 2, `test_world_without_calendar_tables`.
- **A malformed `calendar_days.csv` row** (day 0, day 31, two rows on the same day). The kernel must reject it loudly at init, not mis-place observances. Pinned in Task 2, `test_month_day_validation`.
- **Many months into the game** (year 2+, day 361, day 3600). The moon must stay in step with the day of the month. Pinned in Task 2, `test_moon_cycle_repeats_every_month`.
- **Console commands with bad arguments** (`sim.Give` without a quantity, an unknown place for `sim.Teleport`). Each must print its usage and do nothing. Pinned in Task 7, the `Sim.Console.BadArgs` automation test.

---

### Task 1: Tommy's city runs on Linux (A1)

**Files:**
- Verify: `unreal/Source/SchizoGame/Private/SimEnvironment.cpp`, `SimMeshKit.cpp`, `SimDayNight.cpp`
- Possibly modify: whichever of these fails on Linux (guard Windows-only paths with `#if PLATFORM_WINDOWS`)
- Modify: `HANDOFF.md` (append a "Linux check of b5a0e5e" note)

**Interfaces:**
- Consumes: nothing.
- Produces: a known-good Linux build of `main`, which every later task builds on.

- [ ] **Step 1: Rebuild the kernel for UE**

Run: `UE_ROOT=$HOME/UnrealEngine tools/build_kernel_for_ue.sh`
Expected: ends by writing `kernel/build-ue/libsim_core.a`, with no errors.

- [ ] **Step 2: Build the editor target**

Run: `~/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh SchizoGameEditor Linux Development -Project="$PWD/unreal/SchizoGame.uproject" 2>&1 | tail -20`
Expected: `Result: Succeeded`. If it fails in `SimEnvironment`/`SimMeshKit`/`SimDayNight`, read the error. The likely causes are MSVC-only syntax, or a Windows-only API outside the existing `#if !PLATFORM_LINUX` blocks. Fix only the failing line and keep Tommy's Windows behaviour identical.

- [ ] **Step 3: Generate the style materials headless (M_Flat, M_FlatWind, M_Water, M_Glow)**

Run: `~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd "$PWD/unreal/SchizoGame.uproject" -run=pythonscript -script="$PWD/tools/art/ue_make_style_materials.py" -unattended -nullrhi 2>&1 | tail -15`
Expected: the script logs each material saved; `git status unreal/Content` shows the new or updated `.uasset` files (now tracked by LFS).

- [ ] **Step 4: Headless boot**

Run: `timeout 900 ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor "$PWD/unreal/SchizoGame.uproject" -game -nullrhi -unattended -ExecCmds="quit" -log 2>&1 | grep -E "Sim world created|Error|Fatal" | head`
Expected: `Sim world created from ...: day 1, season rains, hour 6.00.` and no `Fatal`.

- [ ] **Step 5: A windowed launch on the iGPU**

Run: `VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json SDL_VIDEODRIVER=x11 ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor "$PWD/unreal/SchizoGame.uproject" -game -windowed -ResX=1280 -ResY=720 -SimShots=/tmp/claude-1000/shots -SimShotHours=8,20 &`. Then wait. The first launch compiles pipelines for 10–15 minutes.
Expected: screenshots in `/tmp/claude-1000/shots` show the city, walls and ziggurat by day and by night; `journalctl -k --since "-20 min" | grep -i "gpu reset"` is empty. Read two screenshots to confirm.

- [ ] **Step 6: Record and commit**

Append to `HANDOFF.md`:
```markdown
## Linux check of Tommy's b5a0e5e (2026-09-2x)
- Builds on Linux/UE 5.8.3; style materials generated headless; `-game -nullrhi` boots to day 1; the iGPU windowed launch shows the whole city by day and night with no GPU reset. <list any guard added here>
```
```bash
git add HANDOFF.md unreal/Content unreal/Source
git commit -m "A1: Tommy's city verified on Linux (build, headless boot, iGPU launch)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: The kernel calendar learns the moon and the days of the month (A14, kernel)

**Files:**
- Modify: `kernel/include/sim/Time.hpp`
- Modify: `kernel/src/Time.cpp`
- Test: `kernel/tests/test_time.cpp`

**Interfaces:**
- Consumes: the existing `Calendar`, `CalendarConfig`, `Date`, `DayNumber`.
- Produces (used by Tasks 3–5):
  ```cpp
  enum class MoonPhase { New, Waxing, Full, Waning };
  const char* moon_phase_id(MoonPhase p);          // "new" | "waxing" | "full" | "waning"
  struct CalendarDayDef { Id id; std::string name; int day_of_month = 1; std::string omen; std::string deity; };
  // CalendarConfig gains: std::vector<CalendarDayDef> month_days;
  MoonPhase Calendar::moon_phase(DayNumber day) const;
  int Calendar::moon_illumination(DayNumber day) const;      // 0..100
  const CalendarDayDef* Calendar::month_day(DayNumber day) const;  // nullptr when none
  ```

- [ ] **Step 1: Write the failing tests** — append to `kernel/tests/test_time.cpp` before `SIM_MAIN`, and add each name to the `SIM_MAIN(...)` list:

```cpp
static bool test_moon_phase_through_a_month() {
    Calendar cal;  // 30-day months: new on day 1, full on day 15
    SIM_CHECK(cal.moon_phase(1) == MoonPhase::New);
    SIM_CHECK(cal.moon_phase(2) == MoonPhase::Waxing);
    SIM_CHECK(cal.moon_phase(14) == MoonPhase::Waxing);
    SIM_CHECK(cal.moon_phase(15) == MoonPhase::Full);
    SIM_CHECK(cal.moon_phase(16) == MoonPhase::Waning);
    SIM_CHECK(cal.moon_phase(30) == MoonPhase::Waning);
    SIM_CHECK_EQ(std::string(moon_phase_id(MoonPhase::Full)), "full");
    return true;
}

static bool test_moon_illumination_is_integer_and_bounded() {
    Calendar cal;
    SIM_CHECK_EQ(cal.moon_illumination(1), 0);
    SIM_CHECK_EQ(cal.moon_illumination(8), 50);
    SIM_CHECK_EQ(cal.moon_illumination(15), 100);
    SIM_CHECK_EQ(cal.moon_illumination(16), 93);
    SIM_CHECK_EQ(cal.moon_illumination(30), 6);
    for (DayNumber d = 1; d <= 60; ++d) {
        const int i = cal.moon_illumination(d);
        SIM_CHECK(i >= 0 && i <= 100);
    }
    return true;
}

static bool test_moon_cycle_repeats_every_month() {
    Calendar cal;
    for (DayNumber d : {DayNumber{1}, DayNumber{15}, DayNumber{23}}) {
        SIM_CHECK(cal.moon_phase(d) == cal.moon_phase(d + 30));
        SIM_CHECK(cal.moon_phase(d) == cal.moon_phase(d + 3600));
        SIM_CHECK_EQ(cal.moon_illumination(d), cal.moon_illumination(d + 30));
    }
    SIM_CHECK(cal.moon_phase(361) == MoonPhase::New);  // year 2, month 1, day 1
    return true;
}

static bool test_month_days_by_day_of_month() {
    CalendarConfig cfg;
    cfg.month_days = {{"new_crescent", "Day of the New Crescent", 1, "favourable", "nanna"},
                      {"ibbu", "Ibbû, the day of wrath", 19, "unfavourable", ""}};
    Calendar cal(cfg);
    SIM_CHECK(cal.month_day(1) != nullptr);
    SIM_CHECK_EQ(cal.month_day(1)->id, "new_crescent");
    SIM_CHECK_EQ(cal.month_day(31)->id, "new_crescent");   // month 2, day 1
    SIM_CHECK_EQ(cal.month_day(49)->omen, "unfavourable"); // month 2, day 19
    SIM_CHECK(cal.month_day(2) == nullptr);
    return true;
}

static bool test_month_day_validation() {
    auto throws = [](int dom_a, int dom_b) {
        CalendarConfig cfg;
        cfg.month_days = {{"a", "A", dom_a, "", ""}, {"b", "B", dom_b, "", ""}};
        try { Calendar cal(cfg); } catch (const std::invalid_argument&) { return true; }
        return false;
    };
    SIM_CHECK(throws(0, 2));    // day 0
    SIM_CHECK(throws(1, 31));   // past the month
    SIM_CHECK(throws(7, 7));    // two rows on one day
    SIM_CHECK(!throws(1, 30));  // both ends are legal
    return true;
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake -S kernel -B /tmp/claude-1000/sim-build-main -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/claude-1000/sim-build-main --target test_time 2>&1 | tail -5`
Expected: compile errors: `MoonPhase` was not declared, `month_days` has no member.

- [ ] **Step 3: Implement.** In `kernel/include/sim/Time.hpp`, after `FestivalDef`, add:

```cpp
// The moon (A14, D-024 §7): the month is lunar. Day 1 is the new crescent,
// the middle day (15 of 30) is full; the phase is pure day-of-month
// arithmetic, so it needs no state and replays identically (D-022).
enum class MoonPhase { New, Waxing, Full, Waning };
const char* moon_phase_id(MoonPhase p);  // "new" | "waxing" | "full" | "waning"

// A day of every month with a name, an omen and/or an observance
// (db/canon/calendar_days.csv): the eššešu moon days, the unfavourable days of
// the hemerologies. Same day_of_month in every month.
struct CalendarDayDef {
    Id id;
    std::string name;
    int day_of_month = 1;  // 1..days_per_month
    std::string omen;      // "favourable" | "unfavourable" | ""
    std::string deity;     // deity id honoured that day, "" when none
};
```

Add to `CalendarConfig`: `std::vector<CalendarDayDef> month_days;  // calendar_days.csv (A14)`

Add to `class Calendar` (public, after `festivals()`):

```cpp
    MoonPhase moon_phase(DayNumber day) const;
    // Integer 0..100: 0 at the new crescent, 100 at full (D-022: no floats).
    int moon_illumination(DayNumber day) const;
    // The calendar_days row for this day of the month, or nullptr.
    const CalendarDayDef* month_day(DayNumber day) const;
```

In `kernel/src/Time.cpp`, at the end of the constructor body add:

```cpp
    std::sort(cfg_.month_days.begin(), cfg_.month_days.end(),
              [](const CalendarDayDef& a, const CalendarDayDef& b) {
                  return a.day_of_month < b.day_of_month;
              });
    for (std::size_t i = 0; i < cfg_.month_days.size(); ++i) {
        const int dom = cfg_.month_days[i].day_of_month;
        if (dom < 1 || dom > cfg_.days_per_month)
            throw std::invalid_argument("calendar day outside the month");
        if (i > 0 && cfg_.month_days[i - 1].day_of_month == dom)
            throw std::invalid_argument("two calendar days on the same day_of_month");
    }
```

and append the definitions (inside `namespace sim`):

```cpp
const char* moon_phase_id(MoonPhase p) {
    switch (p) {
        case MoonPhase::New: return "new";
        case MoonPhase::Waxing: return "waxing";
        case MoonPhase::Full: return "full";
        case MoonPhase::Waning: return "waning";
    }
    return "new";
}

MoonPhase Calendar::moon_phase(DayNumber day) const {
    const int dom = to_date(day).day;
    const int full = cfg_.days_per_month / 2;
    if (dom == 1) return MoonPhase::New;
    if (dom < full) return MoonPhase::Waxing;
    if (dom == full) return MoonPhase::Full;
    return MoonPhase::Waning;
}

int Calendar::moon_illumination(DayNumber day) const {
    const int dom = to_date(day).day;
    const int dpm = cfg_.days_per_month;
    const int full = dpm / 2;
    if (full <= 1) return 100;  // degenerate shapes: always lit
    if (dom <= full) return (dom - 1) * 100 / (full - 1);
    return (dpm + 1 - dom) * 100 / (dpm + 1 - full);
}

const CalendarDayDef* Calendar::month_day(DayNumber day) const {
    const int dom = to_date(day).day;
    const auto it = std::lower_bound(cfg_.month_days.begin(), cfg_.month_days.end(), dom,
        [](const CalendarDayDef& d, int v) { return d.day_of_month < v; });
    return (it != cfg_.month_days.end() && it->day_of_month == dom) ? &*it : nullptr;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build /tmp/claude-1000/sim-build-main -j && ctest --test-dir /tmp/claude-1000/sim-build-main --output-on-failure 2>&1 | tail -3`
Expected: `100% tests passed, 0 tests failed out of 40`.

- [ ] **Step 5: Commit**

```bash
git add kernel/include/sim/Time.hpp kernel/src/Time.cpp kernel/tests/test_time.cpp
git commit -m "A14: the kernel calendar learns the moon (phase, integer illumination) and named days of the month

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: The calendar canon — month names and the days of the month (A14, data)

**Files:**
- Create: `db/canon/months.csv`, `db/canon/calendar_days.csv`
- Create: `db/schema/months.md`, `db/schema/calendar_days.md`
- Create: `docs/proposals/invented-ledger-calendar.md`
- Modify: `kernel/src/Db.cpp:90-104` (the table list), `kernel/src/World.cpp:19-39` (calendar config)
- Test: `kernel/tests/test_world.cpp`

**Interfaces:**
- Consumes: `CalendarDayDef`, `CalendarConfig::month_days`, `CalendarConfig::month_names` (Task 2).
- Produces: `WorldState::cal` with 12 named months and 8 month days, loaded from the canon.

- [ ] **Step 1: Write the canon.** `db/canon/months.csv` is the machine-readable form of `calendar.csv:month_names`, as `festivals.csv` is for `festival_days`:

```csv
id,number,name,tag,source_ref
rains_coming,1,Rains-Coming,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
waters_rising,2,Waters-Rising,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
silt_settling,3,Silt-Settling,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
barley_sown,4,Barley-Sown,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
wheat_sown,5,Wheat-Sown,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
green_furrow,6,Green-Furrow,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
first_cutting,7,First-Cutting,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
grain_home,8,Grain-Home,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
threshing,9,Threshing,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
ishtars_torch_month,10,Ishtar's-Torch,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
press_moon,11,Press-Moon,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
dead_fires,12,Dead-Fires,INVENTED,"D-018; calendar.csv:month_names (machine-readable form, D-024 §7)"
```

`db/canon/calendar_days.csv`: the practice is attested (tag `A`); that the omens *work* in play is the game's (`INVENTED: EFFECT`, the codex says so):

```csv
id,day_of_month,name,omen,deity,tag,source_ref
new_crescent,1,Day of the New Crescent (eššešu of Nanna),favourable,nanna,A,"Ur III eššešu festival days 1/7/15 of the moon god at Ur (W. Sallaberger, Der kultische Kalender der Ur III-Zeit, 1993); the moon month begins at first visibility of the crescent"
seventh_day,7,Seventh-Day eššešu,unfavourable,nanna,A,"eššešu on day 7 (Sallaberger 1993); day 7 among the unfavourable days of the hemerologies (A. Livingstone, Hemerologies of Assyrian and Babylonian Scholars, 2013)"
fourteenth_day,14,Fourteenth Day,unfavourable,,A,"hemerologies: days 7/14/19/21/28 unfavourable (Livingstone 2013)"
sapattu,15,Šapattu (the full moon's rest and eššešu),favourable,nanna,A,"šapattu = the 15th, the full-moon day (CAD Š/1 s.v. šapattu); eššešu on day 15 (Sallaberger 1993)"
ibbu,19,Ibbû (the day of wrath),unfavourable,,A,"the 19th = 49th day from the previous new moon, the ibbû day of wrath (Livingstone 2013)"
twenty_first_day,21,Twenty-First Day,unfavourable,,A,"hemerologies: days 7/14/19/21/28 unfavourable (Livingstone 2013)"
twenty_eighth_day,28,Twenty-Eighth Day,unfavourable,,A,"hemerologies: days 7/14/19/21/28 unfavourable (Livingstone 2013)"
bubbulum,30,Bubbulum (the moon's disappearance),unfavourable,nanna,A,"bubbulum/ūm bubbuli, the day of the moon's invisibility at month's end (CAD B s.v. bubbulu)"
```

- [ ] **Step 2: Write the schema docs** in the style of `db/schema/festivals.md`:

`db/schema/months.md`:
```markdown
# months

**Rows:** 12 · **Source:** the machine-readable form of `calendar.csv:month_names` (INVENTED, D-018); D-024 §7 — [docs/proposals/invented-ledger-calendar.md](../../docs/proposals/invented-ledger-calendar.md).

## Fields

| Field | Meaning |
|---|---|
| id | month id |
| number | 1..12, the month's place in the year (Time.hpp) |
| name | the name shown to the player |
| tag | INVENTED |
| source_ref | where it comes from |
```

`db/schema/calendar_days.md`:
```markdown
# calendar_days

**Rows:** 8 · **Source:** the attested Mesopotamian moon days and hemerologies (A); D-024 §7 — [docs/proposals/invented-ledger-calendar.md](../../docs/proposals/invented-ledger-calendar.md).

## Fields

| Field | Meaning |
|---|---|
| id | day id |
| day_of_month | 1..30, the same day in every month |
| name | the name shown to the player |
| omen | favourable, unfavourable, or empty |
| deity | deity id honoured that day (deities.csv), or empty |
| tag | A: the practice is attested; its effect in play is INVENTED: EFFECT |
| source_ref | the scholarly source |
```

`docs/proposals/invented-ledger-calendar.md`:
```markdown
# Invented ledger — the calendar turned on (D-024 §7)

The designer may veto any row (a veto = a superseding DECISIONS entry).

| Row | Tag | What was decided | Why it fits |
|---|---|---|---|
| months.csv (12) | INVENTED | the month names of calendar.csv made machine-readable | already ratified in calendar.csv; nothing new named |
| calendar_days.csv (8) | A | the eššešu moon days (1, 7, 15) of Nanna, the šapattu full moon, the unfavourable days 7/14/19/21/28, bubbulum on the 30th | the City of the Moon is Nanna's city (Ur); these are the attested moon-cult and hemerology days |
| moon phase arithmetic | INVENTED | new on day 1, full on day 15, integer illumination | a 30-day lunar month with the crescent at day 1 matches the attested practice closely enough for play |
| omens in play | INVENTED: EFFECT | favourable and unfavourable days will change rites, trade and events (Stage O, step O1) | the hemerologies are real; that they work is the game's world |
```

- [ ] **Step 3: Write the failing world test.** Append to `kernel/tests/test_world.cpp` and register it in `SIM_MAIN`:

```cpp
static bool test_world_calendar_from_canon() {
    WorldState w;
    w.init("../db/canon", 7);
    SIM_CHECK_EQ(w.cal.month_name(w.cal.to_date(1)), "Rains-Coming");
    SIM_CHECK_EQ(w.cal.month_name(w.cal.to_date(360)), "Dead-Fires");
    SIM_CHECK(w.cal.month_day(1) != nullptr);
    SIM_CHECK_EQ(w.cal.month_day(1)->deity, "nanna");
    SIM_CHECK_EQ(w.cal.month_day(19)->omen, "unfavourable");
    SIM_CHECK(w.cal.month_day(2) == nullptr);
    return true;
}

static bool test_world_without_calendar_tables() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "sim_canon_no_calendar";
    fs::remove_all(dir);
    fs::copy("../db/canon", dir);
    fs::remove(dir / "months.csv");
    fs::remove(dir / "calendar_days.csv");
    WorldState w;
    w.init(dir.string(), 7);  // must not throw
    SIM_CHECK(w.cal.month_name(w.cal.to_date(1)).empty());
    SIM_CHECK(w.cal.month_day(1) == nullptr);
    w.advance_days(2);
    fs::remove_all(dir);
    return true;
}
```

Add `#include <filesystem>` at the top of `test_world.cpp` if it is not there.

Run: `cmake --build /tmp/claude-1000/sim-build-main --target test_world && (cd kernel && /tmp/claude-1000/sim-build-main/test_world)`
Expected: `FAIL ... month_name(...) == "Rains-Coming"` (the kernel does not load the tables yet).

- [ ] **Step 4: Wire the tables.** In `kernel/src/Db.cpp`, add `"months", "calendar_days",  // A14 (D-024 §7)` to the `tables` vector. In `kernel/src/World.cpp`, replace the comment line `// D-018). Months stay unnamed in the kernel (month names are not wired).` with `// D-018). Month names are months.csv; named days of the month are calendar_days.csv (A14).`, and before `cal = Calendar{cfg};` insert:

```cpp
    // months.csv: all 12 non-OPEN rows by number, or none (a partial list
    // would misname the year, so the months then stay unnamed numbers).
    {
        std::vector<std::string> names(static_cast<std::size_t>(cfg.months_per_year));
        int named = 0;
        for (const Row& r : db.rows("months")) {
            if (r.get("tag") == "OPEN") continue;
            const long n = std::strtol(r.get("number", "0").c_str(), nullptr, 10);
            if (n >= 1 && n <= cfg.months_per_year && names[static_cast<std::size_t>(n - 1)].empty()) {
                names[static_cast<std::size_t>(n - 1)] = r.get("name");
                ++named;
            }
        }
        if (named == cfg.months_per_year) cfg.month_names = names;
    }
    for (const Row& r : db.rows("calendar_days")) {
        if (r.get("tag") == "OPEN") continue;
        CalendarDayDef d;
        d.id = r.at("id");
        d.name = r.get("name");
        d.day_of_month = static_cast<int>(std::strtol(r.get("day_of_month", "0").c_str(), nullptr, 10));
        d.omen = r.get("omen");
        d.deity = r.get("deity");
        cfg.month_days.push_back(d);  // Calendar validates range + one-per-day
    }
```

- [ ] **Step 5: Run the full gate**

Run: `cmake --build /tmp/claude-1000/sim-build-main -j && ctest --test-dir /tmp/claude-1000/sim-build-main --output-on-failure | tail -3 && python3 tools/canon_lint.py | tail -1 && python3 tools/coverage_check.py | tail -1 && python3 tools/stage_canon_for_ue.py && python3 tools/stage_canon_for_ue.py --check; echo exit=$?`
Expected: `100% tests passed`, `LINT PASSED`, `COVERAGE PASSED`, `exit=0`.

- [ ] **Step 6: Commit**

```bash
git add db/canon/months.csv db/canon/calendar_days.csv db/schema/months.md db/schema/calendar_days.md docs/proposals/invented-ledger-calendar.md kernel/src/Db.cpp kernel/src/World.cpp kernel/tests/test_world.cpp unreal/Content/Sim/canon
git commit -m "A14: the calendar canon — month names wired, the moon days of Nanna and the unfavourable days (D-024 §7)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: The calendar through the C API (A14, C boundary)

**Files:**
- Modify: `kernel/include/sim/CApi.h` (after the festival block, ~line 222)
- Modify: `kernel/src/CApi.cpp` (after `sim_world_festival_on`, ~line 651)
- Test: `kernel/tests/test_capi.cpp`

**Interfaces:**
- Consumes: `Calendar::to_date`, `month_name`, `moon_phase`, `moon_illumination`, `month_day`, `moon_phase_id` (Tasks 2–3).
- Produces (used by Task 5):
  ```c
  int sim_world_date(const SimWorld* world, int* year, int* month, int* day_of_month); // 0 ok, -1 null
  int sim_world_month_name(const SimWorld* world, char* out, int cap);
  int sim_world_moon_phase(const SimWorld* world, char* out, int cap);
  int sim_world_moon_illumination(const SimWorld* world);                             // 0..100, -1 null
  int sim_world_day_omen(const SimWorld* world, char* out, int cap);
  int sim_world_day_observance(const SimWorld* world, char* out, int cap);
  ```

- [ ] **Step 1: Write the failing tests** (append to `kernel/tests/test_capi.cpp`, register them in `SIM_MAIN`):

```cpp
static bool test_calendar_through_c() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    if (!w) return false;
    int y = 0, m = 0, d = 0;
    SIM_CHECK_EQ(sim_world_date(w, &y, &m, &d), 0);
    SIM_CHECK(y == 1 && m == 1 && d == 1);
    char buf[128];
    SIM_CHECK(sim_world_month_name(w, buf, sizeof buf) > 0);
    SIM_CHECK_EQ(std::string(buf), "Rains-Coming");
    sim_world_moon_phase(w, buf, sizeof buf);
    SIM_CHECK_EQ(std::string(buf), "new");
    SIM_CHECK_EQ(sim_world_moon_illumination(w), 0);
    sim_world_day_omen(w, buf, sizeof buf);
    SIM_CHECK_EQ(std::string(buf), "favourable");
    SIM_CHECK(sim_world_day_observance(w, buf, sizeof buf) > 0);
    SIM_CHECK(std::strstr(buf, "Nanna") != nullptr);

    sim_world_advance_days(w, 1);  // day 2: an ordinary day
    SIM_CHECK_EQ(sim_world_day_omen(w, buf, sizeof buf), 0);
    SIM_CHECK_EQ(sim_world_day_observance(w, buf, sizeof buf), 0);

    sim_world_advance_days(w, 13);  // day 15: full moon
    sim_world_moon_phase(w, buf, sizeof buf);
    SIM_CHECK_EQ(std::string(buf), "full");
    SIM_CHECK_EQ(sim_world_moon_illumination(w), 100);

    sim_world_advance_days(w, 4);  // day 19: ibbû
    sim_world_day_omen(w, buf, sizeof buf);
    SIM_CHECK_EQ(std::string(buf), "unfavourable");
    sim_world_destroy(w);
    return true;
}

static bool test_calendar_null_and_small_buffers() {
    int y, m, d;
    char buf[4];
    SIM_CHECK_EQ(sim_world_date(nullptr, &y, &m, &d), -1);
    SIM_CHECK_EQ(sim_world_moon_illumination(nullptr), -1);
    SIM_CHECK_EQ(sim_world_moon_phase(nullptr, buf, sizeof buf), -1);
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK_EQ(sim_world_date(w, nullptr, nullptr, nullptr), -1);
    const int len = sim_world_month_name(w, buf, sizeof buf);  // "Rains-Coming" truncated
    SIM_CHECK_EQ(len, 12);
    SIM_CHECK_EQ(std::string(buf), "Rai");
    sim_world_destroy(w);
    return true;
}

static bool test_calendar_survives_save_load() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    sim_world_advance_days(w, 44);  // day 45 = month 2, day 15
    const std::string path = (std::filesystem::temp_directory_path() / "sim_cal_save.txt").string();
    SIM_CHECK_EQ(sim_world_save(w, path.c_str()), 0);
    SimWorld* l = sim_world_load("../db/canon", path.c_str());
    SIM_CHECK(l != nullptr);
    int y, m, d;
    sim_world_date(l, &y, &m, &d);
    SIM_CHECK(m == 2 && d == 15);
    char buf[64];
    sim_world_month_name(l, buf, sizeof buf);
    SIM_CHECK_EQ(std::string(buf), "Waters-Rising");
    SIM_CHECK_EQ(sim_world_moon_illumination(l), 100);
    sim_world_destroy(l);
    sim_world_destroy(w);
    return true;
}
```

Before relying on `sim_world_save` returning 0, check its documented success value in `CApi.h` (around line 62) and match it.

Run: `cmake --build /tmp/claude-1000/sim-build-main --target test_capi 2>&1 | tail -3`
Expected: `use of undeclared identifier 'sim_world_date'`.

- [ ] **Step 2: Declare** in `kernel/include/sim/CApi.h`, after the festival/market block:

```c
// Calendar (A14, D-024 §7) ---------------------------------------------------
// Today's date: year (1-based), month 1..12, day of month 1..30. Returns 0,
// or -1 on a null world or any null out-pointer.
int sim_world_date(const SimWorld* world, int* year, int* month, int* day_of_month);
// Today's month name (months.csv), "" when the canon names no months.
int sim_world_month_name(const SimWorld* world, char* out, int cap);
// Today's moon: "new" | "waxing" | "full" | "waning".
int sim_world_moon_phase(const SimWorld* world, char* out, int cap);
// Today's moon illumination, 0 (new) .. 100 (full); -1 on a null world.
int sim_world_moon_illumination(const SimWorld* world);
// Today's omen from calendar_days.csv: "favourable" | "unfavourable" | "".
int sim_world_day_omen(const SimWorld* world, char* out, int cap);
// Today's named day of the month (e.g. the eššešu of Nanna), "" when none.
int sim_world_day_observance(const SimWorld* world, char* out, int cap);
```

- [ ] **Step 3: Implement** in `kernel/src/CApi.cpp`, after `sim_world_festival_on`:

```cpp
int sim_world_date(const SimWorld* world, int* year, int* month, int* day_of_month) {
    try {
        if (world == nullptr || year == nullptr || month == nullptr || day_of_month == nullptr) return -1;
        const sim::Date d = world->world.cal.to_date(world->world.day);
        *year = d.year;
        *month = d.month;
        *day_of_month = d.day;
        return 0;
    } catch (...) {
        return -1;
    }
}

int sim_world_month_name(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        const auto& cal = world->world.cal;
        return write_str(out, cap, cal.month_name(cal.to_date(world->world.day)));
    } catch (...) {
        return -1;
    }
}

int sim_world_moon_phase(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        return write_str(out, cap, sim::moon_phase_id(world->world.cal.moon_phase(world->world.day)));
    } catch (...) {
        return -1;
    }
}

int sim_world_moon_illumination(const SimWorld* world) {
    try {
        if (world == nullptr) return -1;
        return world->world.cal.moon_illumination(world->world.day);
    } catch (...) {
        return -1;
    }
}

int sim_world_day_omen(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        const sim::CalendarDayDef* d = world->world.cal.month_day(world->world.day);
        return write_str(out, cap, d ? d->omen : std::string());
    } catch (...) {
        return -1;
    }
}

int sim_world_day_observance(const SimWorld* world, char* out, int cap) {
    try {
        if (world == nullptr) return -1;
        const sim::CalendarDayDef* d = world->world.cal.month_day(world->world.day);
        return write_str(out, cap, d ? d->name : std::string());
    } catch (...) {
        return -1;
    }
}
```

`write_str` is already defined in this file's anonymous namespace (line 53). If the file refers to types without `sim::`, match the file.

- [ ] **Step 4: Run the gate**

Run: `cmake --build /tmp/claude-1000/sim-build-main -j && ctest --test-dir /tmp/claude-1000/sim-build-main --output-on-failure | tail -3`
Expected: `100% tests passed`.

- [ ] **Step 5: Commit**

```bash
git add kernel/include/sim/CApi.h kernel/src/CApi.cpp kernel/tests/test_capi.cpp
git commit -m "A14: the calendar through the C API (date, month, moon, omen, observance)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: The calendar on screen and in the sky (A14, Unreal)

**Files:**
- Modify: `unreal/Plugins/SimRuntime/Source/SimRuntime/Public/SimWorldSubsystem.h`
- Modify: `unreal/Plugins/SimRuntime/Source/SimRuntime/Private/SimWorldSubsystem.cpp`
- Modify: `unreal/Source/SchizoGame/Private/SimHud.cpp:66-82` (the clock block)
- Modify: `unreal/Source/SchizoGame/Private/SimDayNight.cpp:197-212` (`ApplyHour`)
- Test: `unreal/Plugins/SimRuntime/Source/SimRuntime/Private/Tests/SimCalendarTest.cpp`

**Interfaces:**
- Consumes: the six C API calls from Task 4.
- Produces (used by the HUD, the sky, and Task 7's `sim.Dump`):
  ```cpp
  static bool    USimWorldSubsystem::GetSimDateFor(const UObject*, int32& Year, int32& Month, int32& DayOfMonth);
  static FString USimWorldSubsystem::GetSimMonthNameFor(const UObject*);
  static FString USimWorldSubsystem::GetSimMoonPhaseFor(const UObject*);
  static int32   USimWorldSubsystem::GetSimMoonIlluminationFor(const UObject*);  // -1 before the world exists
  static FString USimWorldSubsystem::GetSimDayOmenFor(const UObject*);
  static FString USimWorldSubsystem::GetSimDayObservanceFor(const UObject*);
  ```

- [ ] **Step 1: Write the failing automation test** `unreal/Plugins/SimRuntime/Source/SimRuntime/Private/Tests/SimCalendarTest.cpp`. It drives the C API with the staged canon, so it needs no map:

```cpp
// SimCalendarTest.cpp — A14: the calendar reaches the engine intact.
#include "Misc/AutomationTest.h"
#include "SimGameInstanceSubsystem.h"
#include "sim/CApi.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCalendarTest, "Sim.Calendar.MoonAndMonth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSimCalendarTest::RunTest(const FString& Parameters)
{
	const FString Canon = USimGameInstanceSubsystem::CanonDir();
	SimWorld* W = sim_world_create(TCHAR_TO_UTF8(*Canon), 42);
	if (!TestNotNull(TEXT("world from the staged canon"), W))
	{
		return false;
	}
	char Buf[128] = {};
	sim_world_month_name(W, Buf, sizeof(Buf));
	TestEqual(TEXT("month 1"), FString(UTF8_TO_TCHAR(Buf)), FString(TEXT("Rains-Coming")));
	sim_world_advance_days(W, 14);
	sim_world_moon_phase(W, Buf, sizeof(Buf));
	TestEqual(TEXT("day 15 is full"), FString(UTF8_TO_TCHAR(Buf)), FString(TEXT("full")));
	TestEqual(TEXT("full = 100"), sim_world_moon_illumination(W), 100);
	sim_world_destroy(W);
	return true;
}

#endif
```

- [ ] **Step 2: Build and run it to see it fail**

Run: `UE_ROOT=$HOME/UnrealEngine tools/build_kernel_for_ue.sh && ~/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh SchizoGameEditor Linux Development -Project="$PWD/unreal/SchizoGame.uproject" 2>&1 | tail -3 && timeout 900 ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd "$PWD/unreal/SchizoGame.uproject" -nullrhi -unattended -ExecCmds="Automation RunTests Sim.Calendar;Quit" -log 2>&1 | grep -E "Test Completed|Result=" | head`
Expected: before the kernel rebuild the link fails on `sim_world_month_name`; after it, the test passes. If it passes at once, the test is still valid: it guards the engine-side canon path.

- [ ] **Step 3: Add the getters.** In `SimWorldSubsystem.h`, in the world-context section:

```cpp
	// --- the calendar (A14, D-024 §7) ---------------------------------------
	/** Today's date. False (outputs untouched) before the world exists. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static bool GetSimDateFor(const UObject* WorldContextObject, int32& Year, int32& Month, int32& DayOfMonth);

	/** Month name (months.csv); empty before the world exists or when months are unnamed. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static FString GetSimMonthNameFor(const UObject* WorldContextObject);

	/** "new" | "waxing" | "full" | "waning"; empty before the world exists. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static FString GetSimMoonPhaseFor(const UObject* WorldContextObject);

	/** 0 (new) .. 100 (full); -1 before the world exists. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static int32 GetSimMoonIlluminationFor(const UObject* WorldContextObject);

	/** "favourable" | "unfavourable" | "" (calendar_days.csv). */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static FString GetSimDayOmenFor(const UObject* WorldContextObject);

	/** Today's named day of the month (e.g. the eššešu of Nanna), or empty. */
	UFUNCTION(BlueprintPure, Category = "Sim|Calendar", meta = (WorldContext = "WorldContextObject"))
	static FString GetSimDayObservanceFor(const UObject* WorldContextObject);
```

In `SimWorldSubsystem.cpp`, add a helper to the second anonymous namespace (after `HandleIn`):

```cpp
	/** Reads one C API string writer into an FString ("" when the world is missing). */
	FString ReadSimString(const SimWorld* Handle, int (*Fn)(const SimWorld*, char*, int))
	{
		if (Handle == nullptr)
		{
			return FString();
		}
		char Buf[256] = {};
		return Fn(Handle, Buf, sizeof(Buf)) >= 0 ? FString(UTF8_TO_TCHAR(Buf)) : FString();
	}
```

and at the end of the file:

```cpp
// --- the calendar (A14) -------------------------------------------------------

bool USimWorldSubsystem::GetSimDateFor(const UObject* WorldContextObject, int32& Year, int32& Month, int32& DayOfMonth)
{
	int Y = 0, M = 0, D = 0;
	if (sim_world_date(HandleIn(WorldOf(WorldContextObject)), &Y, &M, &D) != 0)
	{
		return false;
	}
	Year = Y;
	Month = M;
	DayOfMonth = D;
	return true;
}

FString USimWorldSubsystem::GetSimMonthNameFor(const UObject* WorldContextObject)
{
	return ReadSimString(HandleIn(WorldOf(WorldContextObject)), &sim_world_month_name);
}

FString USimWorldSubsystem::GetSimMoonPhaseFor(const UObject* WorldContextObject)
{
	return ReadSimString(HandleIn(WorldOf(WorldContextObject)), &sim_world_moon_phase);
}

int32 USimWorldSubsystem::GetSimMoonIlluminationFor(const UObject* WorldContextObject)
{
	return sim_world_moon_illumination(HandleIn(WorldOf(WorldContextObject)));
}

FString USimWorldSubsystem::GetSimDayOmenFor(const UObject* WorldContextObject)
{
	return ReadSimString(HandleIn(WorldOf(WorldContextObject)), &sim_world_day_omen);
}

FString USimWorldSubsystem::GetSimDayObservanceFor(const UObject* WorldContextObject)
{
	return ReadSimString(HandleIn(WorldOf(WorldContextObject)), &sim_world_day_observance);
}
```

(`sim_world_date` and `sim_world_moon_illumination` already return -1 on a null handle, so no extra check is needed.)

- [ ] **Step 4: The HUD's date lines.** In `SimHud.cpp`, replace the body of the `if (Day >= 0 && Hour >= 0.f)` clock block with:

```cpp
			const int32 H = FMath::Clamp(FMath::FloorToInt(Hour), 0, 23);
			const int32 M = FMath::Clamp(FMath::FloorToInt((Hour - H) * 60.f), 0, 59);
			int32 Year = 1, Month = 1, Dom = 1;
			USimWorldSubsystem::GetSimDateFor(GetWorld(), Year, Month, Dom);
			const FString MonthName = USimWorldSubsystem::GetSimMonthNameFor(GetWorld());
			const FText Line1 = FText::Format(NSLOCTEXT("SimHud", "ClockLine", "{0} {1}, year {2}  -  {3}  -  {4}"),
				FText::AsNumber(Dom),
				MonthName.IsEmpty() ? FText::AsNumber(Month) : FText::FromString(MonthName),
				FText::AsNumber(Year),
				FText::FromString(USimWorldSubsystem::GetSimSeasonFor(GetWorld())),
				FText::FromString(FString::Printf(TEXT("%02d:%02d"), H, M)));

			const FString Phase = USimWorldSubsystem::GetSimMoonPhaseFor(GetWorld());
			const FString Omen = USimWorldSubsystem::GetSimDayOmenFor(GetWorld());
			const FString Observance = USimWorldSubsystem::GetSimDayObservanceFor(GetWorld());
			FText MoonText = NSLOCTEXT("SimHud", "MoonWaning", "Waning moon");
			if (Phase == TEXT("new")) MoonText = NSLOCTEXT("SimHud", "MoonNew", "New crescent");
			else if (Phase == TEXT("waxing")) MoonText = NSLOCTEXT("SimHud", "MoonWaxing", "Waxing moon");
			else if (Phase == TEXT("full")) MoonText = NSLOCTEXT("SimHud", "MoonFull", "Full moon");
			FText Line2 = MoonText;
			if (Omen == TEXT("favourable")) Line2 = FText::Format(NSLOCTEXT("SimHud", "Favourable", "{0}  -  a favourable day"), Line2);
			else if (Omen == TEXT("unfavourable")) Line2 = FText::Format(NSLOCTEXT("SimHud", "Unfavourable", "{0}  -  an unfavourable day"), Line2);
			if (!Observance.IsEmpty()) Line2 = FText::Format(NSLOCTEXT("SimHud", "Observance", "{0}  -  {1}"), Line2, FText::FromString(Observance));

			float LineY = 36.f;
			for (const FText& Line : {Line1, Line2})
			{
				float W = 0.f, Hh = 0.f;
				Canvas->StrLen(GEngine->GetMediumFont(), Line.ToString(), W, Hh);
				FCanvasTextItem Item(FVector2D(Canvas->ClipX - W - 40.f, LineY), Line,
					GEngine->GetMediumFont(), FLinearColor(0.95f, 0.9f, 0.8f));
				Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f), FVector2D(1.f, 1.f));
				Canvas->DrawItem(Item);
				LineY += Hh + 4.f;
			}
```

- [ ] **Step 5: The moon follows its phase.** In `SimDayNight.cpp` `ApplyHour`, replace the line `const FVector M = BodyDir(SunPhase + PI + 0.3f, kMoonTiltDeg);` with:

```cpp
	// The moon's place in the sky follows its phase (A14): with the sun at the
	// new crescent (day 1), opposite it at full (day 15). Illumination scales
	// its light; a dark-moon night stays dark (real darkness, city-life §8).
	int32 Year = 1, Month = 1, Dom = 15;
	USimWorldSubsystem::GetSimDateFor(this, Year, Month, Dom);
	const float MoonOffset = 2.f * PI * static_cast<float>(Dom - 1) / 28.f;
	const int32 Illum = USimWorldSubsystem::GetSimMoonIlluminationFor(this);
	const float MoonLit = Illum < 0 ? 1.f : FMath::Max(0.05f, Illum / 100.f);
	const FVector M = BodyDir(SunPhase + MoonOffset, kMoonTiltDeg);
```

and change `Moon->SetIntensity(kMoonLux * MoonUp);` to `Moon->SetIntensity(kMoonLux * MoonUp * MoonLit);`.

- [ ] **Step 6: Build, test, look**

Run: the build command from Step 2, then `timeout 900 ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd "$PWD/unreal/SchizoGame.uproject" -nullrhi -unattended -ExecCmds="Automation RunTests Sim.;Quit" -log 2>&1 | grep -E "Test Completed|Result="`
Expected: `Sim.Calendar.MoonAndMonth` Result={Success}. Then run the iGPU windowed launch from Task 1 Step 5 with `-SimShotHours=12,23`: the top-right shows two lines (for example `1 Rains-Coming, year 1 - rains - 12:00` and `New crescent - a favourable day - Day of the New Crescent (eššešu of Nanna)`). Read the screenshot to confirm.

- [ ] **Step 7: Commit**

```bash
git add unreal/Plugins/SimRuntime unreal/Source/SchizoGame/Private/SimHud.cpp unreal/Source/SchizoGame/Private/SimDayNight.cpp
git commit -m "A14: the calendar on screen (date, month, moon, omen, observance) and the moon placed and lit by its phase

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: A 48-minute day by default (A4, DQ2)

**Files:**
- Modify: `unreal/Plugins/SimRuntime/Source/SimRuntime/Public/SimWorldSubsystem.h` (the `SimDaysPerRealMinute` default and its comment)
- Modify: `unreal/Plugins/SimRuntime/Source/SimRuntime/Private/SimRuntimeModule.cpp:8` (the comment)
- Modify: `docs/plan.md:97` (the bring-up TODOs line)

**Interfaces:**
- Consumes: nothing.
- Produces: `USimWorldSubsystem::SimDaysPerRealMinute == 48.0f`. The settings menu (plan part 3, A11) will write this same value through the `sim.DaysPerRealMinute` cvar.

- [ ] **Step 1: Check that no ini overrides the default**

Run: `grep -rn "SimDaysPerRealMinute" unreal/Config/`
Expected: no output. If a line exists, change its value to 48 instead of editing the header default.

- [ ] **Step 2: Change the default.** In `SimWorldSubsystem.h`, set `float SimDaysPerRealMinute = 48.0f;` and change the comment `default: 45 — one sim day every 45 real minutes` to `default: 48 (D-024 §2) — one sim day every 48 real minutes, 2 real minutes per game hour; the settings menu sets it`. Update the matching comment in `SimRuntimeModule.cpp:8`.

- [ ] **Step 3: Close the TODOs in plan.md.** Replace the sentence in `docs/plan.md` beginning `Two observations carried as bring-up TODOs:` with: `Both bring-up TODOs are closed: (a) the double initialisation is handled by lazy creation on the first begun-play tick (only the world that begins play creates the sim); (b) the day length is adjustable (config + sim.DaysPerRealMinute), default 48 real minutes (D-024).`

- [ ] **Step 4: Build and verify**

Run: the build from Task 5 Step 2, then `timeout 900 ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor "$PWD/unreal/SchizoGame.uproject" -game -nullrhi -unattended -ExecCmds="sim.DaysPerRealMinute 0.05" -log 2>&1 | grep -m3 "Sim day"` (stop it with Ctrl-C after the lines appear).
Expected: `Sim day 2.` appears within seconds, proving the cvar still overrides the default.

- [ ] **Step 5: Commit**

```bash
git add unreal/Plugins/SimRuntime docs/plan.md
git commit -m "A4: the sim day is 48 real minutes by default (D-024 §2); bring-up TODOs closed

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: The developer console (A5)

**Files:**
- Modify: `kernel/include/sim/CApiVerbs.h`, `kernel/src/CApiVerbs.cpp` (a new `sim_world_set_need`)
- Test: `kernel/tests/test_capi_verbs.cpp`
- Create: `unreal/Plugins/SimRuntime/Source/SimRuntime/Private/SimConsole.cpp`
- Create: `unreal/Source/SchizoGame/Private/SimStreetConsole.cpp`
- Test: `unreal/Plugins/SimRuntime/Source/SimRuntime/Private/Tests/SimConsoleTest.cpp`
- Modify: `docs/completion-plan.md` (the A5 row: later stages add their own commands)

**Interfaces:**
- Consumes: the existing C API (`sim_world_give_item`, `sim_world_add_standing`, `sim_world_add_favour`, `sim_world_set_drought`, `sim_world_set_war`, `sim_world_standing`, `sim_world_favour`, `sim_world_hunger/thirst/fatigue`); `USimWorldSubsystem::SkipSimHoursFor`, `AdvanceSimDaysFor`, and the Task 5 calendar getters; `ASimStreetBuilder::GetPlaceLocation`, `GetAllPlaceIds`.
- Produces:
  ```c
  // CApiVerbs.h: sets one need of one actor to value (clamped 0..100).
  // need is "hunger" | "thirst" | "fatigue". 0 ok, -1 on null/unknown need.
  int sim_world_set_need(SimWorld* world, const char* actor, const char* need, int value);
  ```
  Console commands: `sim.AdvanceDays N`, `sim.AdvanceHours N`, `sim.Give <item> [qty=1] [actor=player]`, `sim.SetNeed <hunger|thirst|fatigue> <0-100> [actor=player]`, `sim.Standing <faction> <value>`, `sim.Favour <deity> <value>`, `sim.Drought <0-5>`, `sim.War <stage>`, `sim.Dump`, `sim.Teleport <place_id>`, `sim.Places`. **Rule for every later stage:** each stage adds the console commands for its own system to `SimConsole.cpp` (or `SimStreetConsole.cpp` when it needs the street).

- [ ] **Step 1: Write the failing kernel test** (append to `kernel/tests/test_capi_verbs.cpp`, add `#include "sim/CApiVerbs.h"` under its `#include "sim/CApi.h"`, and register it in `SIM_MAIN`):

```cpp
static bool test_set_need_for_debugging() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK_EQ(sim_world_set_need(w, "player", "hunger", 80), 0);
    SIM_CHECK_EQ(sim_world_hunger(w, "player"), 80);
    SIM_CHECK_EQ(sim_world_set_need(w, "player", "thirst", 250), 0);  // clamped
    SIM_CHECK_EQ(sim_world_thirst(w, "player"), 100);
    SIM_CHECK_EQ(sim_world_set_need(w, "player", "fatigue", -5), 0);
    SIM_CHECK_EQ(sim_world_fatigue(w, "player"), 0);
    SIM_CHECK_EQ(sim_world_set_need(w, "player", "joy", 5), -1);
    SIM_CHECK_EQ(sim_world_set_need(nullptr, "player", "hunger", 5), -1);
    SIM_CHECK_EQ(sim_world_set_need(w, nullptr, "hunger", 5), -1);
    sim_world_destroy(w);
    return true;
}
```

Run: `cmake --build /tmp/claude-1000/sim-build-main --target test_capi_verbs 2>&1 | tail -2`
Expected: `undeclared identifier 'sim_world_set_need'`.

- [ ] **Step 2: Implement.** Declare it in `kernel/include/sim/CApiVerbs.h` inside the `extern "C"` block with the comment from Interfaces above. In `kernel/src/CApiVerbs.cpp`:

```cpp
int sim_world_set_need(SimWorld* world, const char* actor, const char* need, int value) {
    try {
        if (world == nullptr || actor == nullptr || need == nullptr) return -1;
        sim::Needs& n = sim::needs_of(world->world.needs, actor);
        const int v = value < 0 ? 0 : (value > 100 ? 100 : value);
        const std::string which = need;
        if (which == "hunger") n.hunger = v;
        else if (which == "thirst") n.thirst = v;
        else if (which == "fatigue") n.fatigue = v;
        else return -1;
        return 0;
    } catch (...) {
        return -1;
    }
}
```

Match the file's existing includes and namespace usage (`CApiInternal.hpp` gives `SimWorld`'s definition). Run the kernel gate. Expected: all pass.

- [ ] **Step 3: The SimRuntime console.** Create `unreal/Plugins/SimRuntime/Source/SimRuntime/Private/SimConsole.cpp`:

```cpp
// SimConsole.cpp — A5: developer commands for every live kernel system.
// Each later stage adds the commands for its own system here (completion-plan A5).
// Bad arguments print the usage line and change nothing.
#include "SimWorldSubsystem.h"
#include "SimRuntimeModule.h"
#include "sim/CApi.h"
#include "sim/CApiVerbs.h"

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

namespace
{
	bool NeedArgs(const TArray<FString>& Args, int32 Min, const TCHAR* Usage)
	{
		if (Args.Num() < Min)
		{
			UE_LOG(LogSimRuntime, Warning, TEXT("usage: %s"), Usage);
			return false;
		}
		return true;
	}

	SimWorld* Handle(UWorld* World)
	{
		SimWorld* H = USimWorldSubsystem::GetSimHandleFor(World);
		if (H == nullptr)
		{
			UE_LOG(LogSimRuntime, Warning, TEXT("sim: the world does not exist yet"));
		}
		return H;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdAdvanceDays(TEXT("sim.AdvanceDays"), TEXT("sim.AdvanceDays N — run the world N days"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.AdvanceDays N")) || Handle(World) == nullptr) return;
			USimWorldSubsystem::AdvanceSimDaysFor(World, FMath::Max(0, FCString::Atoi(*Args[0])));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAdvanceHours(TEXT("sim.AdvanceHours"), TEXT("sim.AdvanceHours N — move the clock N game hours"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.AdvanceHours N")) || Handle(World) == nullptr) return;
			USimWorldSubsystem::SkipSimHoursFor(World, FMath::Max(0.f, FCString::Atof(*Args[0])));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdGive(TEXT("sim.Give"), TEXT("sim.Give <item> [qty=1] [actor=player]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.Give <item> [qty=1] [actor=player]"))) return;
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			const int32 Qty = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
			const FString Actor = Args.Num() > 2 ? Args[2] : TEXT("player");
			const int Now = sim_world_give_item(H, TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Args[0]), Qty);
			UE_LOG(LogSimRuntime, Display, TEXT("sim.Give: %s now holds %d %s"), *Actor, Now, *Args[0]);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSetNeed(TEXT("sim.SetNeed"), TEXT("sim.SetNeed <hunger|thirst|fatigue> <0-100> [actor=player]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 2, TEXT("sim.SetNeed <hunger|thirst|fatigue> <0-100> [actor=player]"))) return;
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			const FString Actor = Args.Num() > 2 ? Args[2] : TEXT("player");
			if (sim_world_set_need(H, TCHAR_TO_UTF8(*Actor), TCHAR_TO_UTF8(*Args[0]), FCString::Atoi(*Args[1])) != 0)
			{
				UE_LOG(LogSimRuntime, Warning, TEXT("usage: sim.SetNeed <hunger|thirst|fatigue> <0-100> [actor=player]"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdStanding(TEXT("sim.Standing"), TEXT("sim.Standing <faction> <0-100> — set standing"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 2, TEXT("sim.Standing <faction> <0-100>"))) return;
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			const auto F = StringCast<ANSICHAR>(*Args[0]);
			sim_world_add_standing(H, F.Get(), FCString::Atoi(*Args[1]) - sim_world_standing(H, F.Get()));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdFavour(TEXT("sim.Favour"), TEXT("sim.Favour <deity> <0-100> — set favour"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 2, TEXT("sim.Favour <deity> <0-100>"))) return;
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			const auto D = StringCast<ANSICHAR>(*Args[0]);
			sim_world_add_favour(H, D.Get(), FCString::Atoi(*Args[1]) - sim_world_favour(H, D.Get()));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDrought(TEXT("sim.Drought"), TEXT("sim.Drought <stage>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.Drought <stage>")) || Handle(World) == nullptr) return;
			USimWorldSubsystem::SetSimDroughtFor(World, FCString::Atoi(*Args[0]));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdWar(TEXT("sim.War"), TEXT("sim.War <stage>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!NeedArgs(Args, 1, TEXT("sim.War <stage>"))) return;
			if (SimWorld* H = Handle(World)) sim_world_set_war(H, FCString::Atoi(*Args[0]));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDump(TEXT("sim.Dump"), TEXT("sim.Dump — print the world's vital signs"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			SimWorld* H = Handle(World);
			if (H == nullptr) return;
			int32 Y = 0, M = 0, D = 0;
			USimWorldSubsystem::GetSimDateFor(World, Y, M, D);
			UE_LOG(LogSimRuntime, Display, TEXT("sim: day %lld (%d %s, year %d) %.2fh season=%s moon=%s(%d%%) omen=%s observance=%s"),
				USimWorldSubsystem::GetSimDayFor(World), D, *USimWorldSubsystem::GetSimMonthNameFor(World), Y,
				USimWorldSubsystem::GetSimHourFor(World), *USimWorldSubsystem::GetSimSeasonFor(World),
				*USimWorldSubsystem::GetSimMoonPhaseFor(World), USimWorldSubsystem::GetSimMoonIlluminationFor(World),
				*USimWorldSubsystem::GetSimDayOmenFor(World), *USimWorldSubsystem::GetSimDayObservanceFor(World));
			UE_LOG(LogSimRuntime, Display, TEXT("sim: player hunger=%d thirst=%d fatigue=%d drought=%d war=%d npcs=%d"),
				sim_world_hunger(H, "player"), sim_world_thirst(H, "player"), sim_world_fatigue(H, "player"),
				sim_world_drought(H), sim_world_war(H), sim_world_npc_count(H));
		}));
}
```

Check each C API name and signature against `CApi.h`/`CApiVerbs.h` before building (for example, whether `sim_world_standing` takes `const SimWorld*`), and adjust the calls to match.

- [ ] **Step 4: The street console.** Create `unreal/Source/SchizoGame/Private/SimStreetConsole.cpp`:

```cpp
// SimStreetConsole.cpp — A5: console commands that need the built street.
#include "SimStreetBuilder.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSimStreetConsole, Log, All);

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdPlaces(TEXT("sim.Places"), TEXT("sim.Places — list every place id the street registered"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*)
		{
			TArray<FName> Ids;
			ASimStreetBuilder::GetAllPlaceIds(Ids);
			for (const FName& Id : Ids)
			{
				UE_LOG(LogSimStreetConsole, Display, TEXT("%s"), *Id.ToString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdTeleport(TEXT("sim.Teleport"), TEXT("sim.Teleport <place_id> — move the player to a registered place"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FVector Where;
			if (Args.Num() < 1 || !ASimStreetBuilder::GetPlaceLocation(FName(*Args[0]), Where))
			{
				UE_LOG(LogSimStreetConsole, Warning, TEXT("usage: sim.Teleport <place_id> (see sim.Places)"));
				return;
			}
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (Pawn != nullptr)
			{
				Pawn->TeleportTo(Where + FVector(0.f, 0.f, 120.f), Pawn->GetActorRotation());
			}
		}));
}
```

Check the class name in `SimStreetBuilder.h` (it may be `USimStreetBuilder` or a plain class) and use the exact one.

- [ ] **Step 5: The bad-arguments automation test.** Create `unreal/Plugins/SimRuntime/Source/SimRuntime/Private/Tests/SimConsoleTest.cpp`:

```cpp
// SimConsoleTest.cpp — A5: every sim.* command exists; bad args change nothing.
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimConsoleBadArgsTest, "Sim.Console.BadArgs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSimConsoleBadArgsTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : {TEXT("sim.AdvanceDays"), TEXT("sim.AdvanceHours"), TEXT("sim.Give"), TEXT("sim.SetNeed"),
		TEXT("sim.Standing"), TEXT("sim.Favour"), TEXT("sim.Drought"), TEXT("sim.War"), TEXT("sim.Dump")})
	{
		TestNotNull(Name, IConsoleManager::Get().FindConsoleObject(Name));
	}
	// With no args and no world, each must warn and return (no crash).
	AddExpectedError(TEXT("usage:"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("does not exist yet"), EAutomationExpectedErrorFlags::Contains, 0);
	for (const TCHAR* Cmd : {TEXT("sim.Give"), TEXT("sim.SetNeed hunger"), TEXT("sim.Standing"), TEXT("sim.AdvanceDays")})
	{
		IConsoleManager::Get().ProcessUserConsoleInput(Cmd, *GLog, nullptr);
	}
	return true;
}

#endif
```

(`AddExpectedError` with count 0 accepts any number of matching warnings, so the test passes when the commands warn instead of crashing.)

- [ ] **Step 6: Build and run**

Run: the Task 5 Step 2 build, then `timeout 900 ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd "$PWD/unreal/SchizoGame.uproject" -nullrhi -unattended -ExecCmds="Automation RunTests Sim.;Quit" -log 2>&1 | grep -E "Test Completed|Result="`
Expected: `Sim.Calendar.MoonAndMonth` and `Sim.Console.BadArgs` both `Success`. Then boot `-game -nullrhi` with `-ExecCmds="sim.SetNeed hunger 90;sim.Give bread 3;sim.AdvanceHours 5;sim.Dump;quit"` and check the log shows `hunger=` above 90 and the new date line.

- [ ] **Step 7: Update the plan and commit.** In `docs/completion-plan.md` change the A5 row's step text to: `**Developer console**: sim.AdvanceDays, sim.AdvanceHours, sim.Give, sim.SetNeed, sim.Standing, sim.Favour, sim.Drought, sim.War, sim.Dump, sim.Teleport, sim.Places for the systems live today; **every later stage adds its own system's commands** (sim.Crime with H, sim.Festival with O, sim.Weather with O6, sim.Kill with I, sim.Spawn with F)`.

```bash
git add kernel/include/sim/CApiVerbs.h kernel/src/CApiVerbs.cpp kernel/tests/test_capi_verbs.cpp unreal/Plugins/SimRuntime unreal/Source/SchizoGame/Private/SimStreetConsole.cpp docs/completion-plan.md
git commit -m "A5: the developer console — time, items, needs, standing, favour, drought, war, dump, teleport

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: CI on every push (A2)

**Files:**
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: the kernel CMake project, `tools/canon_lint.py`, `tools/coverage_check.py`, `tools/stage_canon_for_ue.py --check`.
- Produces: a red or green check on every push and pull request to `main`.

- [ ] **Step 1: Write the workflow**

```yaml
name: ci
on:
  push:
    branches: [main]
  pull_request:
jobs:
  kernel-and-canon:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
        with:
          lfs: false   # binaries are not needed for the kernel or the canon
      - uses: actions/setup-python@v5
        with:
          python-version: "3.12"
      - name: canon lint
        run: python3 tools/canon_lint.py
      - name: coverage
        run: python3 tools/coverage_check.py
      - name: staged canon matches db/canon
        run: python3 tools/stage_canon_for_ue.py --check
      - name: build kernel
        run: cmake -S kernel -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
      - name: kernel tests
        run: ctest --test-dir build --output-on-failure
```

The kernel tests read `../db/canon` relative to `kernel/` (CMake sets `WORKING_DIRECTORY`), so building in `build/` at the repo root is fine.

- [ ] **Step 2: Validate locally.** Run each `run:` line in order from the repo root.
Expected: every step exits 0.

- [ ] **Step 3: Commit, push and watch**

```bash
git add .github/workflows/ci.yml
git commit -m "A2: CI — canon lint, coverage, staged-canon check, kernel build and tests on every push

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin main
gh run watch --exit-status $(gh run list --workflow ci --limit 1 --json databaseId -q '.[0].databaseId')
```
Expected: the run finishes green.

- [ ] **Step 4: Protect main (designer's call).** Ask the designer whether a red CI should block pushes to `main` (a branch-protection rule would also block direct pushes). Until they answer, CI only reports.

---

### Task 9: Housekeeping (A13)

**Files:** none (git state only).

- [ ] **Step 1: List what would be deleted**

Run: `git worktree list; git branch -a`
Expected: the worktrees `art1-kit`, `art2-humans`, `art3-dressing`, `verify-gameplay`; local branches `art/*` and `verify/gameplay`; remote `claude/admiring-babbage-c0jyzr`.

- [ ] **Step 2: Confirm that each is fully merged**

Run: `for b in art/dressing art/humans art/kit-materials verify/gameplay; do echo "$b $(git rev-list --count main..$b)"; done; git rev-list --count main..origin/claude/admiring-babbage-c0jyzr`
Expected: every count is `0`. Stop if any is not 0.

- [ ] **Step 3: Ask the designer**, listing the exact names. Only on a yes:

```bash
for w in art1-kit art2-humans art3-dressing verify-gameplay; do git worktree remove .claude/worktrees/$w; done
git branch -d art/dressing art/humans art/kit-materials verify/gameplay
git push origin --delete claude/admiring-babbage-c0jyzr
```
Keep `backup-local-session`, `wip/ue-*` and `archive/*`: HANDOFF.md names them as specs and references.

---

### Task 10: The batch gate — bug review, fix, push, handoff

- [ ] **Step 1: Review the merged range.** Use the `code-review` skill (or a fresh reviewer) on `git diff <start-sha>..main` for this plan's commits. Look at: C API null safety, the calendar maths at the month edges, HUD text layout at 1280×720, console commands with a missing world.
- [ ] **Step 2: Fix what is confirmed**, each fix with a test, then run the full gate: kernel ctest, lint, coverage, stage check, UE build, `Automation RunTests Sim.`.
- [ ] **Step 3: Update HANDOFF.md** with a "Stage A part 1" section: what landed, the new console commands, the calendar, that Tommy needs `git lfs install` before pulling, and that the next plan is Stage A part 2 (the bridge, A6–A7).
- [ ] **Step 4: Push**

```bash
git add HANDOFF.md
git commit -m "Stage A part 1: reviewed, fixed, handed off

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin main
```
