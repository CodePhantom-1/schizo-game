# Stage A, part 2 — The Bridge — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Everything the kernel knows that a UI must show becomes reachable from Unreal: quests and their journal, the dialogue lines a person can say, NPC names and memories, and the feed of fired events. A report lists every C API call that Unreal doesn't reach yet, so no system can hide.

**Architecture:** One new C header per concern, following the existing `CApiVerbs.h`/`CApiWild.h` split: `CApiQuests.h` (quests, journal, dialogue) and `CApiPeople.h` (NPC names, memory, events feed). The implementations sit in `kernel/src/CApiQuests.cpp`/`CApiPeople.cpp`, use `CApiInternal.hpp`'s `SimWorld`, and wrap every body in the `guard`/`write_req` pattern of `CApiVerbs.cpp`. On the Unreal side, one `UBlueprintFunctionLibrary` (`USimQueryLibrary`, SimRuntime plugin) turns each call into Blueprint-ready USTRUCT arrays for the UI shell (part 3). Console commands and automation tests follow part 1's patterns.

**Tech Stack:** C++20 kernel + ctest; UE 5.8.3 (UBT, Automation); Python 3 for the reach report; GitHub Actions.

**Spec:** [docs/completion-plan.md](../../completion-plan.md) steps A6 and A7, with the A6 ruling recorded in part 1 (wrap per stage what each UI needs; a report instead of 176 wrappers up front).

## Global Constraints

- Every C function is `extern "C"`, declared inside the header's `extern "C"` block and **defined inside the .cpp's `extern "C"` block** (part 1 deferred minor: keep them together). It never throws (`guard`), and returns -1 on a null world or a null required argument. String writers return the full length and truncate to `cap-1`; a null/zero `out` is allowed and just returns the length (`write_req`).
- Index-based listing: `*_count(...)` returns N ≥ 0; `*_at(..., index, ...)` returns -1 when `index` is out of range.
- Mutating calls use the kernel's own pipes (`Actions.hpp` `complete_quest` pays rewards), never raw state writes that skip a rule.
- Gate: kernel ctest, canon_lint, coverage_check, stage_canon --check, UE build, `tools/ue_test.sh`, `tools/ue_smoke.sh`.
- UI-facing text in UE uses `NSLOCTEXT`; ids stay `FString`/`FName`.
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. After the plan: bug review → fix → push → watch CI.

## Review Focus

- **Accepting a quest twice, completing one never accepted, or accepting an unknown id.** Each must return an error code and change nothing. Pinned in Task 1, `test_quest_verbs_refuse_nonsense`.
- **Journal text with commas, quotes or newlines** (the save format is line-based). The journal must round-trip through save/load intact. Pinned in Task 1, `test_journal_survives_save_load`.
- **A quest id or speaker key with spaces or mixed case from the UI.** Dialogue matches a speaker case-insensitively (the module's rule). Quest ids are exact, and an unknown id is an error, not a crash. Pinned in Task 2, `test_dialogue_speaker_keys`.
- **An NPC with no memory, or an index past the end.** The call returns 0 or -1, never garbage. Pinned in Task 3, `test_memory_bounds`.
- **The events feed after hundreds of days.** Indexing stays stable and ordered by day. Pinned in Task 3, `test_event_feed_is_ordered`.

---

### Task 1: Quests and the journal through the C API

**Files:**
- Create: `kernel/include/sim/CApiQuests.h`, `kernel/src/CApiQuests.cpp`
- Test: `kernel/tests/test_capi_quests.cpp`

**Interfaces:**
- Consumes: `QuestState`, `accept`, `advance_stage`, `fail`, `find_active`, `journal` (Quests.hpp); `complete_quest` (Actions.hpp); quests.csv's `name`/`giver`/`kind` via `world.db`.
- Produces:
  ```c
  // list: "available" (defs never accepted, completed or failed) | "active" | "completed" | "failed"
  int sim_world_quest_count(const SimWorld* w, const char* list);
  int sim_world_quest_at(const SimWorld* w, const char* list, int index, char* out, int cap);
  int sim_world_quest_title(const SimWorld* w, const char* quest, char* out, int cap);   // quests.csv name
  int sim_world_quest_giver(const SimWorld* w, const char* quest, char* out, int cap);   // quests.csv giver
  int sim_world_quest_stage(const SimWorld* w, const char* quest, char* out, int cap);   // active only; -1 otherwise
  int64_t sim_world_quest_deadline(const SimWorld* w, const char* quest);                // 0 none; -1 not active
  int sim_world_quest_accept(SimWorld* w, const char* quest);     // 0 ok; -2 unknown; -3 already accepted/done/failed
  int sim_world_quest_advance(SimWorld* w, const char* quest, const char* stage, const char* text); // 1 changed, 0 same, -3 not active
  int sim_world_quest_complete(SimWorld* w, const char* quest);   // 0 ok (rewards paid); -3 not active
  int sim_world_quest_abandon(SimWorld* w, const char* quest);    // 0 ok; -3 not active
  int sim_world_journal_count(const SimWorld* w, const char* quest);
  int sim_world_journal_at(const SimWorld* w, const char* quest, int index, int64_t* day,
                           char* stage, int stage_cap, char* text, int text_cap); // text length, -1 bad index
  ```

- [ ] **Step 1: Write the failing test** `kernel/tests/test_capi_quests.cpp`:

```cpp
// test_capi_quests.cpp — A7: quests and the journal through the C boundary.
#include "sim/CApi.h"
#include "sim/CApiQuests.h"
#include "sim/Test.hpp"

#include <cstring>
#include <filesystem>
#include <string>

static std::string at(const SimWorld* w, const char* list, int i) {
    char b[128];
    return sim_world_quest_at(w, list, i, b, sizeof b) >= 0 ? std::string(b) : std::string("<none>");
}

static bool test_quest_lifecycle() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const int available = sim_world_quest_count(w, "available");
    SIM_CHECK(available > 30);  // quests.csv has 41 rows
    SIM_CHECK_EQ(sim_world_quest_count(w, "active"), 0);
    char b[256];
    SIM_CHECK(sim_world_quest_title(w, "the_outsiders_first_days", b, sizeof b) > 0);
    SIM_CHECK_EQ(std::string(b), "The Outsider's First Days");
    SIM_CHECK(sim_world_quest_giver(w, "the_outsiders_first_days", b, sizeof b) > 0);

    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), 0);
    SIM_CHECK_EQ(sim_world_quest_count(w, "active"), 1);
    SIM_CHECK_EQ(sim_world_quest_count(w, "available"), available - 1);
    SIM_CHECK_EQ(at(w, "active", 0), "the_outsiders_first_days");
    SIM_CHECK(sim_world_quest_deadline(w, "the_outsiders_first_days") == 1 + 3);  // accepted day 1, 3-day deadline
    SIM_CHECK_EQ(sim_world_quest_advance(w, "the_outsiders_first_days", "found_bread", "A baker's boy shared his loaf."), 1);
    SIM_CHECK_EQ(sim_world_quest_advance(w, "the_outsiders_first_days", "found_bread", ""), 0);
    sim_world_quest_stage(w, "the_outsiders_first_days", b, sizeof b);
    SIM_CHECK_EQ(std::string(b), "found_bread");

    SIM_CHECK_EQ(sim_world_journal_count(w, "the_outsiders_first_days"), 2);  // accepted, found_bread
    int64_t day = 0;
    char stage[64], text[256];
    SIM_CHECK(sim_world_journal_at(w, "the_outsiders_first_days", 1, &day, stage, sizeof stage, text, sizeof text) > 0);
    SIM_CHECK(day == 1 && std::string(stage) == "found_bread");
    SIM_CHECK(std::strstr(text, "loaf") != nullptr);

    SIM_CHECK_EQ(sim_world_quest_complete(w, "the_outsiders_first_days"), 0);
    SIM_CHECK_EQ(sim_world_quest_count(w, "completed"), 1);
    SIM_CHECK_EQ(sim_world_journal_count(w, "the_outsiders_first_days"), 3);  // + completed
    sim_world_destroy(w);
    return true;
}

static bool test_deadline_fails_and_lists_it() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), 0);
    sim_world_advance_days(w, 5);  // past its 3-day deadline
    SIM_CHECK_EQ(sim_world_quest_count(w, "active"), 0);
    SIM_CHECK_EQ(sim_world_quest_count(w, "failed"), 1);
    SIM_CHECK_EQ(at(w, "failed", 0), "the_outsiders_first_days");
    sim_world_destroy(w);
    return true;
}

static bool test_quest_verbs_refuse_nonsense() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    SIM_CHECK_EQ(sim_world_quest_accept(w, "no_such_quest"), -2);
    SIM_CHECK_EQ(sim_world_quest_complete(w, "the_outsiders_first_days"), -3);  // never accepted
    SIM_CHECK_EQ(sim_world_quest_abandon(w, "the_outsiders_first_days"), -3);
    SIM_CHECK_EQ(sim_world_quest_advance(w, "the_outsiders_first_days", "x", ""), -3);
    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), 0);
    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), -3);    // twice
    SIM_CHECK_EQ(sim_world_quest_count(w, "active"), 1);
    SIM_CHECK_EQ(sim_world_quest_abandon(w, "the_outsiders_first_days"), 0);
    SIM_CHECK_EQ(sim_world_quest_accept(w, "the_outsiders_first_days"), -3);    // failed stays failed
    SIM_CHECK_EQ(sim_world_quest_count(w, "bogus_list"), -1);
    SIM_CHECK_EQ(sim_world_quest_count(nullptr, "active"), -1);
    SIM_CHECK_EQ(sim_world_quest_accept(w, nullptr), -1);
    char b[8];
    SIM_CHECK_EQ(sim_world_quest_at(w, "active", 0, b, sizeof b), -1);  // index past end
    SIM_CHECK_EQ(sim_world_journal_at(w, "no_such_quest", 0, nullptr, nullptr, 0, nullptr, 0), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_journal_survives_save_load() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    sim_world_quest_accept(w, "the_outsiders_first_days");
    const char* tricky = "He said, \"bread\"; then left, quickly";
    SIM_CHECK_EQ(sim_world_quest_advance(w, "the_outsiders_first_days", "met_baker", tricky), 1);
    const std::string path = (std::filesystem::temp_directory_path() / "sim_journal_save.txt").string();
    SIM_CHECK_EQ(sim_world_save(w, path.c_str()), 0);
    SimWorld* l = sim_world_load("../db/canon", path.c_str());
    SIM_CHECK(l != nullptr);
    int64_t day;
    char stage[64], text[256];
    SIM_CHECK(sim_world_journal_at(l, "the_outsiders_first_days", 1, &day, stage, sizeof stage, text, sizeof text) > 0);
    SIM_CHECK_EQ(std::string(text), tricky);
    sim_world_destroy(l);
    sim_world_destroy(w);
    std::filesystem::remove(path);
    return true;
}

SIM_MAIN(test_quest_lifecycle, test_deadline_fails_and_lists_it, test_quest_verbs_refuse_nonsense,
         test_journal_survives_save_load)
```

Before running, check `the_outsiders_first_days`'s `deadline_days` (3) in `db/canon/quests.csv`, and how `tick_quests` computes the deadline (the accept day plus `deadline_days`), then match `test_quest_lifecycle`'s deadline assertion to it.

- [ ] **Step 2: Run to verify it fails.** `cmake --build /tmp/claude-1000/sim-build-main --target test_capi_quests 2>&1 | grep -m1 error`. Expected: `sim/CApiQuests.h: No such file`.

- [ ] **Step 3: Write `kernel/include/sim/CApiQuests.h`**. Use the Interfaces block's declarations, each with a one-line comment, inside `#ifdef __cplusplus extern "C" { #endif … }`, with `#include <stdint.h>` and `typedef struct SimWorld SimWorld;` exactly as in `CApiVerbs.h`'s preamble (copy its first 20 lines).

- [ ] **Step 4: Write `kernel/src/CApiQuests.cpp`**:

```cpp
// CApiQuests.cpp — A7: quests, the journal and dialogue through the C API.
#include "sim/CApiQuests.h"

#include "sim/Actions.hpp"
#include "sim/Dialogue.hpp"
#include "sim/Quests.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "CApiInternal.hpp"

using namespace sim;

namespace {

int write_req(char* out, int cap, const std::string& s) {
    if (out != nullptr && cap > 0) {
        const int n = std::min<int>(cap - 1, static_cast<int>(s.size()));
        std::memcpy(out, s.data(), static_cast<std::size_t>(n));
        out[n] = '\0';
    }
    return static_cast<int>(s.size());
}

template <class F>
int guard(F&& f) {
    try { return f(); } catch (...) { return -1; }
}

bool contains(const std::vector<Id>& v, const Id& id) { return std::find(v.begin(), v.end(), id) != v.end(); }

const Row* quest_row(const WorldState& w, const Id& id) {
    for (const Row& r : w.db.rows("quests"))
        if (r.get("id") == id) return &r;
    return nullptr;
}

// The ids in one named list, in stable order (defs order for "available").
bool list_ids(const WorldState& w, const std::string& list, std::vector<Id>& out) {
    const QuestState& q = w.quests;
    if (list == "active") { for (const Quest& a : q.active) out.push_back(a.def_id); return true; }
    if (list == "completed") { out = q.completed; return true; }
    if (list == "failed") { out = q.failed_list; return true; }
    if (list == "available") {
        for (const QuestDef& d : q.defs)
            if (!find_active(q, d.id) && !contains(q.completed, d.id) && !contains(q.failed_list, d.id))
                out.push_back(d.id);
        return true;
    }
    return false;
}

bool known_def(const WorldState& w, const Id& id) {
    return std::any_of(w.quests.defs.begin(), w.quests.defs.end(), [&](const QuestDef& d) { return d.id == id; });
}

}  // namespace

extern "C" {

int sim_world_quest_count(const SimWorld* w, const char* list) {
    return guard([&] {
        if (w == nullptr || list == nullptr) return -1;
        std::vector<Id> ids;
        return list_ids(w->world, list, ids) ? static_cast<int>(ids.size()) : -1;
    });
}

int sim_world_quest_at(const SimWorld* w, const char* list, int index, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || list == nullptr || index < 0) return -1;
        std::vector<Id> ids;
        if (!list_ids(w->world, list, ids) || index >= static_cast<int>(ids.size())) return -1;
        return write_req(out, cap, ids[static_cast<std::size_t>(index)]);
    });
}

int sim_world_quest_title(const SimWorld* w, const char* quest, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        const Row* r = quest_row(w->world, quest);
        return r ? write_req(out, cap, r->get("name")) : -1;
    });
}

int sim_world_quest_giver(const SimWorld* w, const char* quest, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        const Row* r = quest_row(w->world, quest);
        return r ? write_req(out, cap, r->get("giver")) : -1;
    });
}

int sim_world_quest_stage(const SimWorld* w, const char* quest, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        const Quest* q = find_active(w->world.quests, quest);
        return q ? write_req(out, cap, q->stage) : -1;
    });
}

int64_t sim_world_quest_deadline(const SimWorld* w, const char* quest) {
    try {
        if (w == nullptr || quest == nullptr) return -1;
        const Quest* q = find_active(w->world.quests, quest);
        return q ? static_cast<int64_t>(q->deadline) : -1;
    } catch (...) {
        return -1;
    }
}

int sim_world_quest_accept(SimWorld* w, const char* quest) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        QuestState& q = w->world.quests;
        if (!known_def(w->world, quest)) return -2;
        if (find_active(q, quest) || contains(q.completed, quest) || contains(q.failed_list, quest)) return -3;
        accept(q, quest, w->world.day);
        return 0;
    });
}

int sim_world_quest_advance(SimWorld* w, const char* quest, const char* stage, const char* text) {
    return guard([&] {
        if (w == nullptr || quest == nullptr || stage == nullptr) return -1;
        if (!find_active(w->world.quests, quest)) return -3;
        return advance_stage(w->world.quests, quest, w->world.day, stage, text ? text : "") ? 1 : 0;
    });
}

int sim_world_quest_complete(SimWorld* w, const char* quest) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        if (!find_active(w->world.quests, quest)) return -3;
        complete_quest(w->world, quest, w->world.day);  // pays silver + standing (Actions.hpp)
        return 0;
    });
}

int sim_world_quest_abandon(SimWorld* w, const char* quest) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        if (!find_active(w->world.quests, quest)) return -3;
        fail(w->world.quests, quest);
        return 0;
    });
}

int sim_world_journal_count(const SimWorld* w, const char* quest) {
    return guard([&] {
        if (w == nullptr || quest == nullptr) return -1;
        return static_cast<int>(journal(w->world.quests, quest).size());
    });
}

int sim_world_journal_at(const SimWorld* w, const char* quest, int index, int64_t* day,
                         char* stage, int stage_cap, char* text, int text_cap) {
    return guard([&] {
        if (w == nullptr || quest == nullptr || index < 0) return -1;
        const auto& j = journal(w->world.quests, quest);
        if (index >= static_cast<int>(j.size())) return -1;
        const JournalEntry& e = j[static_cast<std::size_t>(index)];
        if (day != nullptr) *day = static_cast<int64_t>(e.day);
        write_req(stage, stage_cap, e.stage);
        return write_req(text, text_cap, e.text);
    });
}

}  // extern "C"
```

Check: does `fail()` append a "failed" journal entry? If it doesn't, `abandon` is still correct; the journal just won't show it. Also check `Row::get`'s signature and whether `Db::rows` returns rows by reference (so `&r` stays valid), and adjust if needed. **If `test_journal_survives_save_load` fails because the SIMSAVE writer breaks on quotes or commas in the text, the fix belongs in `Snapshot.cpp`'s field escaping, not in the test.** That is a real save bug; fix it at the root and add the assertion to `test_snapshot.cpp` too.

- [ ] **Step 5: Run the suite.** `cmake --build /tmp/claude-1000/sim-build-main -j && ctest --test-dir /tmp/claude-1000/sim-build-main --output-on-failure | tail -3`. Expected: 41 of 41 pass.

- [ ] **Step 6: Commit.** `git add kernel/include/sim/CApiQuests.h kernel/src/CApiQuests.cpp kernel/tests/test_capi_quests.cpp` (plus `Snapshot.cpp`/`test_snapshot.cpp` if Step 4's note applied). Message: `A7: quests and the journal through the C API (list, accept, advance, complete, abandon, journal)`.

---

### Task 2: Dialogue through the C API

**Files:**
- Modify: `kernel/include/sim/CApiQuests.h`, `kernel/src/CApiQuests.cpp`
- Test: `kernel/tests/test_capi_quests.cpp`

**Interfaces:**
- Consumes: `eligible_lines(db, speaker_key, PlayerContext)` (Dialogue.hpp); `QuestState` for the context.
- Produces:
  ```c
  // Lines `speaker` may say now (dialogues.csv, gated by the player's act via quests).
  int sim_world_dialogue_count(const SimWorld* w, const char* speaker);
  int sim_world_dialogue_at(const SimWorld* w, const char* speaker, int index,
                            char* id, int id_cap, char* text, int text_cap);  // text length; -1 bad index
  ```

- [ ] **Step 1: Failing tests** (append to `test_capi_quests.cpp`, register them in `SIM_MAIN`). The speaker is the most frequent opening-act speaker in dialogues.csv (2 lines):

```cpp
static bool test_dialogue_lines_for_a_speaker() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const char* SPEAKER = "the captain of the prisoner transport";  // 2 opening-act lines
    const int n = sim_world_dialogue_count(w, SPEAKER);
    SIM_CHECK(n >= 1);
    char id[128], text[1024];
    SIM_CHECK(sim_world_dialogue_at(w, SPEAKER, 0, id, sizeof id, text, sizeof text) > 0);
    SIM_CHECK(std::strlen(id) > 0);
    SIM_CHECK_EQ(sim_world_dialogue_at(w, SPEAKER, n, id, sizeof id, text, sizeof text), -1);
    SIM_CHECK_EQ(sim_world_dialogue_count(w, "nobody at all"), 0);
    sim_world_destroy(w);
    return true;
}

static bool test_dialogue_speaker_keys() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const char* SPEAKER = "the captain of the prisoner transport";
    std::string upper = SPEAKER;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    SIM_CHECK_EQ(sim_world_dialogue_count(w, upper.c_str()), sim_world_dialogue_count(w, SPEAKER));
    SIM_CHECK_EQ(sim_world_dialogue_count(w, nullptr), -1);
    SIM_CHECK_EQ(sim_world_dialogue_count(nullptr, SPEAKER), -1);
    sim_world_destroy(w);
    return true;
}
```

Add `#include <cctype>`. Expected failure: `sim_world_dialogue_count` undeclared.

- [ ] **Step 2: Implement** (in `CApiQuests.cpp`, inside `extern "C"`), with a helper in the anonymous namespace:

```cpp
PlayerContext player_context(const WorldState& w) {
    PlayerContext p;
    for (const Quest& q : w.quests.active) p.active_quests.push_back(q.def_id);
    p.completed_quests = w.quests.completed;
    return p;
}
```

```cpp
int sim_world_dialogue_count(const SimWorld* w, const char* speaker) {
    return guard([&] {
        if (w == nullptr || speaker == nullptr) return -1;
        return static_cast<int>(eligible_lines(w->world.db, speaker, player_context(w->world)).size());
    });
}

int sim_world_dialogue_at(const SimWorld* w, const char* speaker, int index,
                          char* id, int id_cap, char* text, int text_cap) {
    return guard([&] {
        if (w == nullptr || speaker == nullptr || index < 0) return -1;
        const auto lines = eligible_lines(w->world.db, speaker, player_context(w->world));
        if (index >= static_cast<int>(lines.size())) return -1;
        const DialogueLine& l = lines[static_cast<std::size_t>(index)];
        write_req(id, id_cap, l.id);
        return write_req(text, text_cap, l.text);
    });
}
```

- [ ] **Step 3: Run the suite and commit.** Expected: all pass. Message: `A7: dialogue lines through the C API`.

---

### Task 3: NPC names, memories and the events feed

**Files:**
- Create: `kernel/include/sim/CApiPeople.h`, `kernel/src/CApiPeople.cpp`
- Test: `kernel/tests/test_capi_people.cpp`

**Interfaces:**
- Consumes: `PopulationState::npcs` (with `Npc::name`, `memory`), `EventsState::fired`, `names.csv`/`people.csv` via db.
- Produces:
  ```c
  int sim_world_npc_name(const SimWorld* w, const char* npc, char* out, int cap);     // display name; -1 unknown npc
  int sim_world_npc_memory_count(const SimWorld* w, const char* npc);                  // -1 unknown npc
  int sim_world_npc_memory_at(const SimWorld* w, const char* npc, int index, int64_t* day,
                              char* subject, int subject_cap, char* fact, int fact_cap);  // fact length; -1 bad index
  int sim_world_event_at(const SimWorld* w, int index, int64_t* day,
                         char* rule, int rule_cap, char* summary, int summary_cap);       // summary length; -1 bad index
  ```

- [ ] **Step 1: Find what a display name is.** Read `people.csv`'s header and `Npc::name`'s loading in `Population.cpp`. If `Npc::name` is already the display name, use it. If it holds an id, resolve it the way the codex does (`tools/codex_gen.py`). Record which in the header comment.

- [ ] **Step 2: Failing tests** `kernel/tests/test_capi_people.cpp`:

```cpp
// test_capi_people.cpp — A7: names, memories and the events feed through C.
#include "sim/CApi.h"
#include "sim/CApiPeople.h"
#include "sim/Test.hpp"

#include <string>

static bool test_npc_names_are_not_ids() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    const int n = sim_world_npc_count(w);
    SIM_CHECK(n > 0);
    char id[128], name[256];
    for (int i = 0; i < n; ++i) {
        SIM_CHECK(sim_world_npc_id(w, i, id, sizeof id) > 0);
        SIM_CHECK(sim_world_npc_name(w, id, name, sizeof name) > 0);
        SIM_CHECK(std::string(name).find('_') == std::string::npos);  // a name, not an id
    }
    SIM_CHECK_EQ(sim_world_npc_name(w, "no_such_npc", name, sizeof name), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_memory_bounds() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    char id[128];
    sim_world_npc_id(w, 0, id, sizeof id);
    const int m = sim_world_npc_memory_count(w, id);
    SIM_CHECK(m >= 0);
    int64_t day;
    char subj[64], fact[256];
    SIM_CHECK_EQ(sim_world_npc_memory_at(w, id, m, &day, subj, sizeof subj, fact, sizeof fact), -1);
    SIM_CHECK_EQ(sim_world_npc_memory_at(w, id, -1, &day, subj, sizeof subj, fact, sizeof fact), -1);
    SIM_CHECK_EQ(sim_world_npc_memory_count(w, "no_such_npc"), -1);
    sim_world_destroy(w);
    return true;
}

static bool test_event_feed_is_ordered() {
    SimWorld* w = sim_world_create("../db/canon", 42);
    sim_world_set_drought(w, 3);
    sim_world_advance_days(w, 400);
    const int n = sim_world_event_count(w);
    SIM_CHECK(n > 0);
    int64_t prev = 0, day = 0;
    char rule[128], summary[512];
    for (int i = 0; i < n; ++i) {
        SIM_CHECK(sim_world_event_at(w, i, &day, rule, sizeof rule, summary, sizeof summary) >= 0);
        SIM_CHECK(day >= prev);
        prev = day;
    }
    SIM_CHECK_EQ(sim_world_event_at(w, n, &day, rule, sizeof rule, summary, sizeof summary), -1);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_npc_names_are_not_ids, test_memory_bounds, test_event_feed_is_ordered)
```

If `test_npc_names_are_not_ids` fails because some NPC's display name really contains an underscore in the canon, that is a data bug: fix the row, not the test. If `test_event_feed_is_ordered` sees `n == 0` after 400 drought days, find out why no event fires before weakening the test; that would be a finding.

- [ ] **Step 3: Implement** `CApiPeople.h` (same preamble as `CApiQuests.h`) and `CApiPeople.cpp`:

```cpp
// CApiPeople.cpp — A7: NPC names, memories and the events feed through C.
#include "sim/CApiPeople.h"

#include "sim/Events.hpp"
#include "sim/Population.hpp"

#include <algorithm>
#include <cstring>
#include <string>

#include "CApiInternal.hpp"

using namespace sim;

namespace {

int write_req(char* out, int cap, const std::string& s) {
    if (out != nullptr && cap > 0) {
        const int n = std::min<int>(cap - 1, static_cast<int>(s.size()));
        std::memcpy(out, s.data(), static_cast<std::size_t>(n));
        out[n] = '\0';
    }
    return static_cast<int>(s.size());
}

template <class F>
int guard(F&& f) {
    try { return f(); } catch (...) { return -1; }
}

const Npc* find_npc(const WorldState& w, const char* id) {
    for (const Npc& n : w.population.npcs)
        if (n.id == id) return &n;
    return nullptr;
}

}  // namespace

extern "C" {

int sim_world_npc_name(const SimWorld* w, const char* npc, char* out, int cap) {
    return guard([&] {
        if (w == nullptr || npc == nullptr) return -1;
        const Npc* n = find_npc(w->world, npc);
        return n ? write_req(out, cap, n->name) : -1;  // per Step 1: the display name
    });
}

int sim_world_npc_memory_count(const SimWorld* w, const char* npc) {
    return guard([&] {
        if (w == nullptr || npc == nullptr) return -1;
        const Npc* n = find_npc(w->world, npc);
        return n ? static_cast<int>(n->memory.size()) : -1;
    });
}

int sim_world_npc_memory_at(const SimWorld* w, const char* npc, int index, int64_t* day,
                            char* subject, int subject_cap, char* fact, int fact_cap) {
    return guard([&] {
        if (w == nullptr || npc == nullptr || index < 0) return -1;
        const Npc* n = find_npc(w->world, npc);
        if (n == nullptr || index >= static_cast<int>(n->memory.size())) return -1;
        const MemoryEntry& m = n->memory[static_cast<std::size_t>(index)];
        if (day != nullptr) *day = static_cast<int64_t>(m.day);
        write_req(subject, subject_cap, m.subject);
        return write_req(fact, fact_cap, m.fact);
    });
}

int sim_world_event_at(const SimWorld* w, int index, int64_t* day,
                       char* rule, int rule_cap, char* summary, int summary_cap) {
    return guard([&] {
        if (w == nullptr || index < 0) return -1;
        const auto& fired = w->world.events.fired;
        if (index >= static_cast<int>(fired.size())) return -1;
        const TriggeredEvent& e = fired[static_cast<std::size_t>(index)];
        if (day != nullptr) *day = static_cast<int64_t>(e.day);
        write_req(rule, rule_cap, e.rule_id);
        return write_req(summary, summary_cap, e.summary);
    });
}

}  // extern "C"
```

If `population.npcs` is a map rather than a vector, adapt `find_npc` to it.

- [ ] **Step 4: Run the suite and commit.** Message: `A7: NPC names, memories and the events feed through the C API`.

---

### Task 4: Unreal — `USimQueryLibrary` (the UI's read and verb surface)

**Files:**
- Create: `unreal/Plugins/SimRuntime/Source/SimRuntime/Public/SimQueryLibrary.h`, `.../Private/SimQueryLibrary.cpp`
- Test: `.../Private/Tests/SimQueryTest.cpp`

**Interfaces:**
- Consumes: the Task 1–3 C calls; `USimWorldSubsystem::GetSimHandleFor`.
- Produces (Blueprint-callable; the part-3 UI builds on these):
  ```cpp
  USTRUCT(BlueprintType) struct FSimQuestInfo { FString Id, Title, Giver, Stage; int64 Deadline = 0; };
  USTRUCT(BlueprintType) struct FSimJournalEntry { int64 Day = 0; FString Stage, Text; };
  USTRUCT(BlueprintType) struct FSimDialogueLine { FString Id, Text; };
  USTRUCT(BlueprintType) struct FSimMemory { int64 Day = 0; FString Subject, Fact; };
  USTRUCT(BlueprintType) struct FSimEventInfo { int64 Day = 0; FString Rule, Summary; };
  UENUM(BlueprintType) enum class ESimQuestList : uint8 { Available, Active, Completed, Failed };
  // USimQueryLibrary (all static, WorldContext):
  TArray<FSimQuestInfo> GetQuests(const UObject* Ctx, ESimQuestList List);
  TArray<FSimJournalEntry> GetJournal(const UObject* Ctx, const FString& QuestId);
  int32 AcceptQuest(const UObject* Ctx, const FString& QuestId);     // C API code
  int32 AdvanceQuest(const UObject* Ctx, const FString& QuestId, const FString& Stage, const FString& Text);
  int32 CompleteQuest(const UObject* Ctx, const FString& QuestId);
  int32 AbandonQuest(const UObject* Ctx, const FString& QuestId);
  TArray<FSimDialogueLine> GetDialogue(const UObject* Ctx, const FString& Speaker);
  FString GetNpcName(const UObject* Ctx, const FString& NpcId);      // falls back to the id
  TArray<FSimMemory> GetNpcMemories(const UObject* Ctx, const FString& NpcId);
  TArray<FSimEventInfo> GetRecentEvents(const UObject* Ctx, int32 MaxCount); // newest first
  ```

- [ ] **Step 1: Failing test** `SimQueryTest.cpp`. Automation tests have no play world, so the library's handle-based functions take an internal overload that accepts a `SimWorld*`. Declare, in `SimQueryLibrary.h`, a plain C++ (non-UFUNCTION) namespace `SimQuery` with one function per UFUNCTION taking `SimWorld*`. The UFUNCTIONs forward to it with `GetSimHandleFor(Ctx)`. Test the namespace functions with a world created from `USimGameInstanceSubsystem::CanonDir()`:

```cpp
// SimQueryTest.cpp — A7: the UI's query surface returns real data.
#include "Misc/AutomationTest.h"
#include "SimGameInstanceSubsystem.h"
#include "SimQueryLibrary.h"
#include "sim/CApi.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimQueryTest, "Sim.Query.QuestsJournalPeople",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSimQueryTest::RunTest(const FString& Parameters)
{
	SimWorld* W = sim_world_create(TCHAR_TO_UTF8(*USimGameInstanceSubsystem::CanonDir()), 42);
	if (!TestNotNull(TEXT("world"), W)) return false;
	const TArray<FSimQuestInfo> Available = SimQuery::GetQuests(W, ESimQuestList::Available);
	TestTrue(TEXT("quests available"), Available.Num() > 30);
	TestEqual(TEXT("accept"), SimQuery::AcceptQuest(W, TEXT("the_outsiders_first_days")), 0);
	const TArray<FSimQuestInfo> Active = SimQuery::GetQuests(W, ESimQuestList::Active);
	TestEqual(TEXT("one active"), Active.Num(), 1);
	TestEqual(TEXT("its title"), Active[0].Title, FString(TEXT("The Outsider's First Days")));
	TestEqual(TEXT("journal has the acceptance"), SimQuery::GetJournal(W, TEXT("the_outsiders_first_days")).Num(), 1);
	char Id[128] = {};
	sim_world_npc_id(W, 0, Id, sizeof(Id));
	TestFalse(TEXT("npc name is not its id"), SimQuery::GetNpcName(W, UTF8_TO_TCHAR(Id)).Contains(TEXT("_")));
	sim_world_advance_days(W, 200);
	const TArray<FSimEventInfo> Recent = SimQuery::GetRecentEvents(W, 5);
	TestTrue(TEXT("at most 5"), Recent.Num() <= 5);
	if (Recent.Num() >= 2) TestTrue(TEXT("newest first"), Recent[0].Day >= Recent[1].Day);
	sim_world_destroy(W);
	return true;
}

#endif
```

Build it and watch it fail (missing header).

- [ ] **Step 2: Implement** the header (the USTRUCTs, UENUM and `UCLASS() class SIMRUNTIME_API USimQueryLibrary : public UBlueprintFunctionLibrary` with `UFUNCTION(BlueprintCallable/BlueprintPure, Category="Sim|Quests", meta=(WorldContext="Ctx"))` declarations, plus `namespace SimQuery { … SimWorld* overloads … }`) and the .cpp. Each list function loops `count`/`at` with 256-byte buffers for ids and 2048-byte buffers for text, converting with `UTF8_TO_TCHAR`. `GetQuests` fills Title/Giver/Stage/Deadline from the Task 1 calls (Stage/Deadline only for Active). `GetRecentEvents` walks from `sim_world_event_count - 1` down. `ESimQuestList` maps to "available"/"active"/"completed"/"failed". Each UFUNCTION is one line: `return SimQuery::X(USimWorldSubsystem::GetSimHandleFor(Ctx), …);`. Every `SimQuery` function returns an empty result or -1 when `W` is null.

- [ ] **Step 3: Rebuild the kernel for UE, build the editor, run `tools/ue_test.sh`.** Expected: 5 tests pass (the new `Sim.Query.QuestsJournalPeople` among them). Commit: `A7: USimQueryLibrary — quests, journal, dialogue, names, memories and events for the UI`.

---

### Task 5: Console commands for the new systems

**Files:** Modify `unreal/Plugins/SimRuntime/Source/SimRuntime/Private/SimConsole.cpp`; test: extend `Tests/SimConsoleTest.cpp`'s name list.

- [ ] **Step 1:** Add `sim.Quests [available|active|completed|failed]`, `sim.Accept <quest>`, `sim.Journal <quest>`, `sim.Talk <speaker…>` (joins all args with spaces), `sim.Memory <npc>` and `sim.Events [n=10]` to the test's name list. Rebuild and watch `Sim.Console.BadArgs` fail.
- [ ] **Step 2:** Implement each using `SimQuery::` (one `UE_LOG(LogSimRuntime, Display, …)` line per item), with usage lines on bad arguments, following the existing commands.
- [ ] **Step 3:** Build and run `tools/ue_test.sh`: all pass. Headless check: `-ExecCmds="sim.Accept the_outsiders_first_days, sim.Quests active, sim.Journal the_outsiders_first_days, sim.Events 3, quit"` prints the quest, its journal line and up to 3 events. Add these as smoke lines to `tools/ue_smoke.sh` (`need 'Accepted|the_outsiders_first_days' …`). Commit: `A5/A7: console — quests, journal, talk, memory, events`.

---

### Task 6: The reach report — no system hides from Unreal (A6)

**Files:**
- Create: `tools/capi_reach.py`, `tools/tests/test_capi_reach.py`
- Create (generated): `docs/capi-reach.md`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Produces: `python3 tools/capi_reach.py [--check]`. It writes `docs/capi-reach.md`: every `sim_world_*` function declared in `kernel/include/sim/CApi*.h`, grouped by header, marked **reached** (referenced anywhere under `unreal/Source` or `unreal/Plugins/*/Source`) or **not yet**, with the completion-plan stage that will reach it (from a small `STAGE_BY_PREFIX` table in the script, e.g. `combat`/`attack`/`wound` → K, `rite`/`omen`/`ward` → J, `buy`/`sell` → C, `craft` → D, `quest`/`journal`/`dialogue` → A/P, fallback "unassigned"). `--check` exits 1 when any function is "unassigned", so every function has a home.

- [ ] **Step 1: Failing test** `tools/tests/test_capi_reach.py` (stdlib `unittest`, run with `python3 -m unittest tools/tests/test_capi_reach.py`):

```python
import pathlib, sys, tempfile, unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import capi_reach

class Reach(unittest.TestCase):
    def test_declarations_and_references(self):
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            (d / "inc").mkdir(); (d / "ue").mkdir()
            (d / "inc" / "CApiX.h").write_text("int sim_world_attack(SimWorld* w);\nint sim_world_frobnicate(const SimWorld* w);\n// sim_world_in_comment(\n")
            (d / "ue" / "A.cpp").write_text("sim_world_attack(H);\n")
            decls = capi_reach.declared(d / "inc")
            self.assertEqual(sorted(f for _, f in decls), ["sim_world_attack", "sim_world_frobnicate"])
            reached = capi_reach.referenced([d / "ue"])
            self.assertIn("sim_world_attack", reached)
            self.assertNotIn("sim_world_frobnicate", reached)
            self.assertEqual(capi_reach.stage_for("sim_world_attack"), "K")
            self.assertEqual(capi_reach.stage_for("sim_world_frobnicate"), "unassigned")

if __name__ == "__main__":
    unittest.main()
```

Run it: fails with `ModuleNotFoundError: capi_reach`.

- [ ] **Step 2: Implement** `tools/capi_reach.py`:
  - `declared(inc_dir)` returns `(header, name)` for each line matching `^\s*[\w\s\*]+?\b(sim_world_\w+)\s*\(` that isn't a comment.
  - `referenced(dirs)` returns the set of `sim_world_\w+` names found in `*.cpp`/`*.h` under the dirs.
  - `stage_for(name)` returns the first `STAGE_BY_PREFIX` substring match, else `"unassigned"`.
  - `main()` writes the markdown table with a summary line ("N of M reached") and implements `--check`.

  Fill `STAGE_BY_PREFIX` until `--check` passes on the real headers, placing every function in its completion-plan stage.

- [ ] **Step 3:** Run the tests and `python3 tools/capi_reach.py --check`: both pass, and `docs/capi-reach.md` is written. Add a CI step, `python3 -m unittest tools/tests/test_capi_reach.py && python3 tools/capi_reach.py --check`. Commit: `A6: the C API reach report — every kernel call has a stage that will bring it to the player`.

---

### Task 7: The batch gate

- [ ] Final whole-batch review by a fresh reviewer (most capable model), with this plan's Review Focus.
- [ ] Fix Critical and Important findings with RED→GREEN tests; ledger the minors.
- [ ] Run the full gate (kernel, canon, `ue_test.sh`, `ue_smoke.sh`, reach `--check`).
- [ ] Append a "Stage A part 2" section to HANDOFF.md. Push, then watch CI.
