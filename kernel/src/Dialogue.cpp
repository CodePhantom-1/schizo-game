// Dialogue.cpp — see sim/Dialogue.hpp for the eligibility rule this module
// invents (speaker match + act-gating from existing columns; D-018).
#include "sim/Dialogue.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>

namespace sim {
namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string lower(const std::string& s) {
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return t;
}

bool is_open(const Row& row) { return trim(row.get("tag")) == "OPEN"; }

// The four acts plus the opening (quests.csv's own `act` column, and every
// dialogues.csv row's `story.csv:<act>` source_ref token use this vocabulary).
// -1 for anything else: unrecognized, so it gates nothing.
int act_rank(const std::string& act) {
    static const std::map<std::string, int> kRank = {
        {"opening", 0}, {"act_i", 1}, {"act_ii", 2}, {"act_iii", 3}, {"act_iv", 4}};
    const auto it = kRank.find(lower(trim(act)));
    return it == kRank.end() ? -1 : it->second;
}

// Pulls the act token out of `...story.csv:<act>...` in source_ref. -1 if the
// token is absent (never gates a line whose provenance isn't act-tagged).
int story_act_rank(const std::string& source_ref) {
    const std::string key = "story.csv:";
    const auto pos = source_ref.find(key);
    if (pos == std::string::npos) return -1;
    const std::size_t start = pos + key.size();
    std::size_t end = start;
    while (end < source_ref.size() &&
           (std::isalnum(static_cast<unsigned char>(source_ref[end])) || source_ref[end] == '_'))
        ++end;
    return act_rank(source_ref.substr(start, end - start));
}

std::string strip_leading_article(const std::string& s) {
    static const char* const kArticles[] = {"a ", "an ", "the "};
    const std::string lowered = lower(s);
    for (const char* a : kArticles) {
        const std::size_t n = std::char_traits<char>::length(a);
        if (lowered.size() >= n && lowered.compare(0, n, a) == 0) return s.substr(n);
    }
    return s;
}

// Two normalized forms: the speaker text as written, and with a leading
// article stripped. A caller's key may match either — a person id like
// "the_prophet" reads as "the prophet" (matches the full form, since the row
// literally says "the Prophet"), while a role key like "water-carrier" only
// matches after "a water-carrier" loses its article.
std::string normalize_full(const std::string& s) { return lower(trim(s)); }
std::string normalize_stripped(const std::string& s) { return lower(trim(strip_leading_article(trim(s)))); }

// A person id or role key: underscores read as spaces ("the_prophet" -> "the
// prophet"), lowercased — matched against normalize_speaker's output.
std::string normalize_key(const std::string& key) {
    std::string k = trim(key);
    for (char& c : k)
        if (c == '_') c = ' ';
    return lower(k);
}

int player_act_rank(const Db& db, const PlayerContext& player) {
    int best = -1;
    const std::vector<Id>* groups[] = {&player.active_quests, &player.completed_quests};
    for (const std::vector<Id>* group : groups) {
        for (const Id& id : *group) {
            const auto row = db.find("quests", id);
            if (!row.has_value()) continue;
            const int r = act_rank(row->get("act"));
            if (r > best) best = r;
        }
    }
    return best < 0 ? 0 : best;  // no quests yet: the opening
}

}  // namespace

std::vector<DialogueLine> eligible_lines(const Db& db, const std::string& speaker_key,
                                          const PlayerContext& player) {
    const std::string wanted = normalize_key(speaker_key);
    const int player_rank = player_act_rank(db, player);

    std::vector<DialogueLine> out;
    for (const Row& row : db.rows("dialogues")) {
        if (is_open(row)) continue;
        if (normalize_speaker(row.get("speaker")) != wanted) continue;
        const int rank = story_act_rank(row.get("source_ref"));
        if (rank >= 0 && rank > player_rank) continue;  // the player hasn't reached this act

        DialogueLine line;
        line.id = row.get("id");
        line.speaker = row.get("speaker");
        line.context = row.get("context");
        line.text = row.get("text");
        out.push_back(std::move(line));
    }
    return out;
}

}  // namespace sim
