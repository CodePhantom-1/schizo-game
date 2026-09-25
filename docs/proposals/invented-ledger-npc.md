# Invented ledger — W2-H, street people, dialogue, and the quest journal

Per D-018: every invented choice below fits theme and canon, contradicts no CANON/A row or the notes, and is subject to designer veto (a veto supersedes here).

- **Light-resident role seeding (`seed_people`).** `Npc` gains a `role` field, set only when a `people.csv` row's `role` column case-insensitively names a real `schedules.csv` role (`matching_schedule_role`, exact match after trim/lowercase). A named leader's narrative title (e.g. "founder of the Empire; the player's previous incarnation") never matches, so Wave 1 behaviour is unchanged; W2-B's 29 street residents (roles authored to match `schedules.csv` exactly) all pick up a role. Purely additive to the existing contract.

- **`npc_task_at` (Population.hpp/.cpp).** Looks up an npc's own `role` and delegates to `Schedule::task_at`. No new eligibility rule beyond "use the npc's own role"; nullopt for an unknown npc or a role-less one.

- **Resident social links (`wire_resident_links`, INVENTED deterministic rule, three parts):**
  1. **Household** — any two residents sharing a non-empty `household` id know each other. `people.csv` has no household column at Wave 1 or in W2-B's street wave, so this rule is presently inert; it activates automatically the day content adds one, with no kernel change.
  2. **Same role, same city** — residents of the same role in the same city all know each other (bakers know bakers).
  3. **Neighbours** — residents of the same city, sorted by id for a stable order, are chained to their immediate id-predecessor: a deterministic stand-in for "lives on the same street" until a real plot/street layout exists (W2-B's `places.csv` gives locations, not adjacency, so this is not yet keyed off it).
  Only npcs with a non-empty `role` participate, which keeps named leaders untouched by construction. Idempotent: re-seeding only ever adds a missing edge.

- **Dialogue runner (`Dialogue.hpp/.cpp`, new module).** `dialogues.csv` (id,speaker,context,text,tag,source_ref) has no gating, condition, choice, or next-line column at all. Two minimal INVENTED rules, built only from existing columns:
  1. **Speaker match** — a caller's key (a `people.csv` id, underscores read as spaces, or a `schedules.csv` role string) is matched case-insensitively against a dialogue row's `speaker` text, either as written or with a leading "a"/"an"/"the" article stripped. Confirmed against all 48 real rows: person ids like `the_prophet` match speaker text like "the Prophet" directly (no stripping needed); role keys like `water-carrier` only match after "a water-carrier" loses its article.
  2. **Act gating** — every dialogues.csv row's `source_ref` carries a `story.csv:<act>` token (opening/act_i/act_ii/act_iii/act_iv, confirmed on all 48 rows). A line is eligible only when its act is at or before the player's current act, computed as the furthest act among `quests.csv`'s own `act` column for the player's active/completed quest ids (default: opening, with none held). A row whose `source_ref` carries no recognizable act token is never gated (always eligible) rather than hidden.
  No choose/advance branching step exists — the data has no choice/next-line column to branch on; documented, not built (Schedule.hpp's festival-hook pattern). `PlayerContext.standing_by_faction` / `.known_facts` are carried for a future rule; no dialogue row references a faction id or a fact today.

- **Quest journal (`Quests.hpp/.cpp`, additive).** New `QuestState::journal` (`map<Id, vector<JournalEntry>>`, keyed by `def_id`, survives the quest leaving `active`). `JournalEntry{day, stage, text}`. Entries are appended by `accept()` ("accepted"), `complete()` ("completed"), a deadline failure inside `tick_quests()` ("failed"), and the new `advance_stage()` (only when the stage actually changes — no duplicate entries for a repeated call). `journal()` read accessor returns a static empty vector for an unknown def_id (never an error).

- **Dangling-reference fix (scenario_quest.md).** `accept()`'s returned `Quest&` still does not survive a subsequent `tick_quests()` call (documented, unchanged, for backward compatibility) — but `find_active(QuestState&, def_id)` / `find_active(const QuestState&, def_id)` give a safe post-tick accessor. `advance_stage()` uses it internally rather than taking a caller-held reference.
