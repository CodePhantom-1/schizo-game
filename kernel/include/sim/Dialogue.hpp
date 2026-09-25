#pragma once
// Dialogue.hpp — the dialogue runner (new module, D-018 creative gap-filling).
// dialogues.csv (id,speaker,context,text,tag,source_ref) has no gating,
// condition, or branching columns: no quest/choice/next-line ids at all. Per
// D-011/D-018 policy this module invents the MINIMUM machinery needed to pick
// eligible lines, built entirely from columns that already exist:
//
//   - `speaker` (prose, e.g. "a water-carrier", "the Prophet") is matched
//     against a caller-given key (a people.csv id, read with underscores as
//     spaces, or a schedules.csv role string) case-insensitively, ignoring a
//     leading "a"/"an"/"the" article on the row's own speaker text. See
//     Dialogue.cpp for the normalize/strip-article rule.
//   - every row's `source_ref` carries a `story.csv:<act>` token (opening,
//     act_i..act_iv — confirmed on all 48 real rows). A line is eligible only
//     when its act is at or before the player's current act, computed as the
//     furthest act among quests.csv's own `act` column for the player's
//     active/completed quest ids (default: "opening" with none held yet).
//     This is the INVENTED eligibility rule: it keeps early-game dialogue
//     from surfacing acts the player hasn't reached, using only data the
//     content authors already wrote.
//
// No choose/advance branching step exists: the data has no choice or
// next-line column to branch on (documented, not built — the same pattern
// Schedule.hpp uses for its festival-override hook).
//
// Standing and known_facts are carried on PlayerContext for a future
// eligibility rule; no dialogue row references a faction id or a fact today,
// so nothing gates on them yet.
#include "sim/Db.hpp"
#include "sim/Types.hpp"

#include <map>
#include <string>
#include <vector>

namespace sim {

struct DialogueLine {
    Id id;
    std::string speaker;
    std::string context;
    std::string text;
};

struct PlayerContext {
    std::vector<Id> active_quests;          // quests.csv ids currently active
    std::vector<Id> completed_quests;       // quests.csv ids completed
    std::map<Id, int> standing_by_faction;  // reserved: no dialogue row keys on faction yet
    std::vector<Id> known_facts;            // reserved: no dialogue row keys on a fact yet
};

// Every dialogues.csv row whose speaker matches `speaker_key` and whose story
// act is at or before the player's current act (see header). Deterministic:
// canon row order. OPEN-tagged rows are always skipped.
std::vector<DialogueLine> eligible_lines(const Db& db, const std::string& speaker_key,
                                          const PlayerContext& player);

}  // namespace sim
