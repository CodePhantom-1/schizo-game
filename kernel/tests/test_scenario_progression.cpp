// test_scenario_progression.cpp — W4-A, the open-play proof (D-021): a fresh
// player reaches EVERY calling, specialisation, second calling and talent in
// the canon through play alone.
//
// "Play alone" means: only the engine's C API verbs a player has — work for a
// resident, train with a teacher, practice, sleep, drink, eat, choose. No
// state is set directly, no quest is touched, no act or story state exists in
// the loop at all. The canon is read (never written) only to learn which
// callings and talents exist and what they ask for, exactly as the UI would
// from data/ue.
#include "sim/CApi.h"
#include "sim/Db.hpp"
#include "sim/Test.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr int kSession = 8;  // hours per work/practice/lesson

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (const char c : s + std::string(1, sep)) {
        if (c == sep) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    return out;
}

std::string read_buf(int (*fn)(const SimWorld*, char*, int), const SimWorld* w) {
    std::vector<char> buf(8192);
    const int n = fn(w, buf.data(), static_cast<int>(buf.size()));
    return n < 0 ? std::string() : std::string(buf.data());
}

struct Plan {
    std::string calling, specialisation, second;
};

struct Player {
    SimWorld* w = nullptr;
    Plan plan;
    std::string dock;  // a silver-paying employer (the wharf)
    int actions = 0;

    int skill(const std::string& id) const { return sim_world_skill(w, "player", id.c_str()); }

    // Sleep the night, drink at the well, eat what the wages bought, and let
    // the world turn a day.
    void rest() {
        sim_world_advance_needs(w, "player", 10, 1);
        sim_world_drink(w, "player", "water");
        if (sim_world_hunger(w, "player") >= 40) {
            for (const char* food : {"bread", "fish", "grain"})
                if (sim_world_eat(w, "player", food) == 0) break;
        }
        sim_world_advance_days(w, 1);
    }

    // The choices a player makes as soon as the levels allow.
    void choose() {
        char buf[64];
        const int level = sim_world_level(w);
        sim_world_calling(w, 0, buf, sizeof buf);
        if (level >= 5 && buf[0] == '\0') (void)sim_world_choose_calling(w, plan.calling.c_str());
        sim_world_calling(w, 1, buf, sizeof buf);
        if (level >= 15 && buf[0] == '\0') (void)sim_world_choose_specialisation(w, plan.specialisation.c_str());
        sim_world_calling(w, 2, buf, sizeof buf);
        if (level >= 25 && buf[0] == '\0') (void)sim_world_choose_second_calling(w, plan.second.c_str());
    }

    // One verb, resting and retrying once when too tired.
    template <class F>
    int act(F f) {
        ++actions;
        int r = f();
        if (r == -6) {
            rest();
            r = f();
        }
        choose();
        return r;
    }

    void earn(std::int64_t silver) {
        while (sim_world_purse(w, "player") < silver)
            if (act([&] { return sim_world_work(w, dock.c_str(), kSession); }) < 0) return;
    }

    // Raise a skill to `target`: a teacher who knows more (paying from wages),
    // else practice while practice still helps.
    bool raise(const std::string& id, int target) {
        for (int guard = 0; guard < 3000; ++guard) {
            const int cur = skill(id);
            if (cur >= target) return true;
            std::vector<char> buf(4096);
            sim_world_skill_teachers(w, id.c_str(), buf.data(), static_cast<int>(buf.size()));
            std::string best;
            int best_max = cur;
            std::int64_t best_fee = 0;
            for (const std::string& entry : split(buf.data(), ';')) {
                const std::vector<std::string> f = split(entry, ':');
                if (f.size() != 3) continue;
                const int mx = std::atoi(f[1].c_str());
                if (mx > best_max) {
                    best = f[0];
                    best_max = mx;
                    best_fee = std::atoll(f[2].c_str());
                }
            }
            if (!best.empty()) {
                earn(best_fee * kSession);
                if (act([&] { return sim_world_train_skill(w, id.c_str(), best.c_str(), kSession); }) < 0)
                    return false;
                continue;
            }
            if (act([&] { return sim_world_practice_skill(w, id.c_str(), kSession); }) < 0) return false;
        }
        return false;
    }
};

struct TalentReq {
    std::string id, calling, skill;
    int min_skill = 0;
};

// Plays one fresh world to level 25+, choosing the plan's callings on the way,
// then takes every talent the plan's callings (and anyone) may take.
bool play(std::uint64_t seed, const Plan& plan, const std::vector<std::string>& skills,
          const std::vector<TalentReq>& talents, std::set<std::string>& taken, std::string* save_out) {
    Player p;
    p.w = sim_world_create("../db/canon", seed);
    if (p.w == nullptr) return false;
    p.plan = plan;
    for (int i = 0; i < sim_world_npc_count(p.w); ++i) {
        char id[128];
        sim_world_npc_id(p.w, i, id, sizeof id);
        std::string sid = id;
        if (sid.find("dockworker") != std::string::npos) p.dock = sid;
    }
    if (p.dock.empty()) { sim_world_destroy(p.w); return false; }

    // Grow broadly until the second calling opens.
    for (int round = 0; round < 20 && sim_world_level(p.w) < 25; ++round)
        for (const std::string& s : skills) {
            if (sim_world_level(p.w) >= 25) break;
            (void)p.raise(s, 5 * (round + 1) > 25 ? 25 : 5 * (round + 1));
        }
    for (int round = 0; round < 10 && sim_world_level(p.w) < 25; ++round)
        for (const std::string& s : skills) (void)p.raise(s, p.skill(s) + 5);
    p.choose();

    char buf[128];
    bool ok = sim_world_level(p.w) >= 25;
    sim_world_calling(p.w, 0, buf, sizeof buf);
    ok = ok && plan.calling == buf;
    sim_world_calling(p.w, 1, buf, sizeof buf);
    ok = ok && plan.specialisation == buf;
    sim_world_calling(p.w, 2, buf, sizeof buf);
    ok = ok && plan.second == buf;
    if (!ok) {
        std::fprintf(stderr, "plan %s/%s/%s not reached (level %d)\n", plan.calling.c_str(),
                     plan.specialisation.c_str(), plan.second.c_str(), sim_world_level(p.w));
        sim_world_destroy(p.w);
        return false;
    }

    const std::set<std::string> held = {plan.calling, plan.specialisation, plan.second};
    for (const TalentReq& t : talents) {
        if (!t.calling.empty() && held.count(t.calling) == 0) continue;
        if (!t.skill.empty() && !p.raise(t.skill, t.min_skill)) {
            std::fprintf(stderr, "talent %s: skill %s stuck at %d < %d\n", t.id.c_str(), t.skill.c_str(),
                         p.skill(t.skill), t.min_skill);
            sim_world_destroy(p.w);
            return false;
        }
        int r = sim_world_choose_talent(p.w, t.id.c_str());
        // Out of talent points: grow further (each level is one more point).
        for (int guard = 0; r == -5 && guard < 200; ++guard) {
            const int level = sim_world_level(p.w);
            for (const std::string& s : skills) {
                (void)p.raise(s, p.skill(s) + 3);
                if (sim_world_level(p.w) > level) break;
            }
            r = sim_world_choose_talent(p.w, t.id.c_str());
        }
        if (r != 0) {
            char why[64];
            sim_world_progression_refusal(p.w, why, sizeof why);
            std::fprintf(stderr, "talent %s refused: %d %s\n", t.id.c_str(), r, why);
            sim_world_destroy(p.w);
            return false;
        }
        taken.insert(t.id);
    }
    if (save_out != nullptr) {
        char probe[1];
        const int n = sim_world_save_to_buffer(p.w, probe, 1);
        std::vector<char> buf2(static_cast<std::size_t>(n) + 1);
        sim_world_save_to_buffer(p.w, buf2.data(), n + 1);
        *save_out = buf2.data();
    }
    sim_world_destroy(p.w);
    return true;
}

}  // namespace

static bool test_every_calling_and_talent_is_reachable_through_play() {
    const sim::Db db = sim::Db::load("../db/canon");
    std::vector<std::string> callings;
    std::vector<std::pair<std::string, std::string>> specs;  // (spec, parent)
    for (const sim::Row& r : db.rows("callings")) {
        if (r.get("tag") == "OPEN") continue;
        if (r.get("kind") == "specialisation") specs.emplace_back(r.at("id"), r.get("parent"));
        else callings.push_back(r.at("id"));
    }
    std::vector<TalentReq> talents;
    for (const sim::Row& r : db.rows("talents")) {
        if (r.get("tag") == "OPEN") continue;
        talents.push_back({r.at("id"), r.get("calling"), r.get("skill"), std::atoi(r.get("min_skill").c_str())});
    }
    std::vector<std::string> skills;
    for (const sim::Row& r : db.rows("skills"))
        if (r.get("tag") != "OPEN") skills.push_back(r.at("id"));
    SIM_CHECK(callings.size() >= 2);
    SIM_CHECK(!specs.empty());
    SIM_CHECK(!talents.empty());

    std::set<std::string> taken, firsts, seconds, specialisations;
    std::uint64_t seed = 100;
    for (const auto& [spec, parent] : specs) {
        std::size_t idx = 0;
        while (idx < callings.size() && callings[idx] != parent) ++idx;
        SIM_CHECK(idx < callings.size());
        const Plan plan{parent, spec, callings[(idx + 1) % callings.size()]};
        SIM_CHECK(play(++seed, plan, skills, talents, taken, nullptr));
        firsts.insert(plan.calling);
        specialisations.insert(plan.specialisation);
        seconds.insert(plan.second);
    }
    SIM_CHECK_EQ(firsts.size(), callings.size());
    SIM_CHECK_EQ(seconds.size(), callings.size());
    SIM_CHECK_EQ(specialisations.size(), specs.size());
    for (const TalentReq& t : talents) {
        if (taken.count(t.id) == 0) std::fprintf(stderr, "talent never reached: %s\n", t.id.c_str());
        SIM_CHECK(taken.count(t.id) == 1);
    }
    return true;
}

// The same play, twice, gives the same save, byte for byte (D-022).
static bool test_play_is_deterministic() {
    const sim::Db db = sim::Db::load("../db/canon");
    std::vector<std::string> skills;
    for (const sim::Row& r : db.rows("skills"))
        if (r.get("tag") != "OPEN") skills.push_back(r.at("id"));
    const Plan plan{"priest", "diviner", "healer"};
    std::set<std::string> taken;
    std::string a, b;
    SIM_CHECK(play(7, plan, skills, {}, taken, &a));
    SIM_CHECK(play(7, plan, skills, {}, taken, &b));
    SIM_CHECK(!a.empty());
    SIM_CHECK_EQ(a, b);
    return true;
}

SIM_MAIN(test_every_calling_and_talent_is_reachable_through_play, test_play_is_deterministic)
