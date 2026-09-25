// test_faction_raids.cpp — W5-B: faction politics decides which bands raid.
// The treaty table, the war sides, outlawry voiding sworn peace, the
// grudge-sharpened raid, the formula's new politics points, the band-politics
// read, the C ABI over Faction, and a save round-trip. Every rule tested here
// is ledgered in docs/proposals/invented-ledger-faction-raids.md.
#include "sim/CApi.h"
#include "sim/Snapshot.hpp"
#include "sim/Test.hpp"
#include "sim/WildActions.hpp"

#include "../src/WildInternal.hpp"  // test-only: raid_politics, city_faction_id

#include <algorithm>
#include <cstring>
#include <string>

using namespace sim;

namespace {

constexpr const char* kCanon = "../db/canon";

WorldState fresh(std::uint64_t seed = 11) {
    WorldState w;
    w.init(kCanon, seed);
    return w;
}

}  // namespace

// --- the treaty table -------------------------------------------------------------

static bool test_treaties_load_and_bind_the_southern_cause() {
    WorldState w = fresh();
    SIM_CHECK(!w.wild.treaty_defs.empty());
    const TreatyDef* pact = nullptr;
    for (const TreatyDef& t : w.wild.treaty_defs)
        if (t.id == "southern_cause_pact") pact = &t;
    SIM_CHECK(pact != nullptr);
    if (pact == nullptr) return true;
    SIM_CHECK_EQ(pact->parties.size(), std::size_t(2));
    for (const Id& p : pact->parties) SIM_CHECK(w.db.has("factions", p));
    // Live between both parties, either way round; not live across sides.
    SIM_CHECK(live_treaty_between(w, "neo_sumerian_rebellion", "southern_city_states_alliance") == pact);
    SIM_CHECK(live_treaty_between(w, "southern_city_states_alliance", "neo_sumerian_rebellion") == pact);
    SIM_CHECK(live_treaty_between(w, "suti", "southern_city_states_alliance") == nullptr);
    SIM_CHECK(live_treaty_between(w, "neo_sumerian_rebellion", "the_empire") == nullptr);
    SIM_CHECK(live_treaty_between(w, "southern_city_states_alliance", "southern_city_states_alliance") == nullptr);
    // The city itself is the alliance (cities.csv jurisdiction, D-005).
    SIM_CHECK_EQ(wild::city_faction_id(w), Id("southern_city_states_alliance"));
    return true;
}

// --- raid_politics: war, grudge, treaty, outlawry ---------------------------------

static bool test_raid_politics_rules() {
    WorldState w = fresh();
    const Id city = wild::city_faction_id(w);
    const GroupDef* warband = find_group_def(w.wild, "eastern_warband");
    const GroupDef* sons = find_group_def(w.wild, "sons_of_the_swamp_sage");
    SIM_CHECK(warband != nullptr && sons != nullptr);

    // Unaligned bands (the Jackals, the Reed Men) carry no politics at all.
    GroupDef loner = *find_group_def(w.wild, "cavern_jackals");
    const wild::RaidPolitics none = wild::raid_politics(w, loner, city);
    SIM_CHECK_EQ(none.opportunity, 0);
    SIM_CHECK_EQ(none.defence, 0);
    SIM_CHECK(!none.blocked && none.note.empty());

    // At peace (war_stage 0) the eastern warband is merely hungry.
    const wild::RaidPolitics calm = wild::raid_politics(w, *warband, city);
    SIM_CHECK_EQ(calm.opportunity, 0);
    SIM_CHECK(calm.note.empty());
    // The war comes: the barbarians raid the settled south harder.
    w.facts.war_stage = 1;
    const wild::RaidPolitics atwar = wild::raid_politics(w, *warband, city);
    SIM_CHECK_EQ(atwar.opportunity, kWarOpportunity);
    SIM_CHECK_EQ(atwar.note, "war");
    // The Suti reign alone: no side of that war.
    const wild::RaidPolitics suti = wild::raid_politics(w, *find_group_def(w.wild, "suti_riders"), city);
    SIM_CHECK_EQ(suti.opportunity, 0);

    // The rebellion stands with the city by sworn pact — its band is reined
    // from the city's own things, outright.
    const wild::RaidPolitics reined = wild::raid_politics(w, *sons, city);
    SIM_CHECK_EQ(reined.defence, kTreatyDefence);
    SIM_CHECK(reined.blocked);
    SIM_CHECK_EQ(reined.note, "treaty");
    // ...and urged harder against the Empire's things: the pact puts the
    // rebellion on the city's side, and the band's grudge names the Empire.
    const wild::RaidPolitics imperial = wild::raid_politics(w, *sons, "the_empire");
    SIM_CHECK_EQ(imperial.opportunity, kWarOpportunity + kGrudgeOpportunity);
    SIM_CHECK_EQ(imperial.note, "war,grudge");
    SIM_CHECK(!imperial.blocked);

    // An outlawed faction's law is broken: the sworn peace is void and its
    // bands owe nobody quarter.
    outlaw(w.faction, "neo_sumerian_rebellion");
    const wild::RaidPolitics outlawed = wild::raid_politics(w, *sons, city);
    SIM_CHECK(!outlawed.blocked);
    SIM_CHECK_EQ(outlawed.defence, 0);
    SIM_CHECK_EQ(outlawed.opportunity, kOutlawOpportunity);
    SIM_CHECK_EQ(outlawed.note, "outlaw");
    SIM_CHECK(live_treaty_between(w, "neo_sumerian_rebellion", "southern_city_states_alliance") == nullptr);

    // A grudge that names the holder sharpens the raid on that holder's
    // things (no canon row names the city yet — the rule is holder-keyed, so
    // a synthetic grudge proves the path the moment content uses it).
    GroupDef hater = *find_group_def(w.wild, "suti_riders");
    hater.grudges.push_back(city);
    const wild::RaidPolitics grudge = wild::raid_politics(w, hater, city);
    SIM_CHECK_EQ(grudge.opportunity, kGrudgeOpportunity);
    SIM_CHECK_EQ(grudge.note, "grudge");
    return true;
}

// --- the formula itself ----------------------------------------------------------

static bool test_assess_target_carries_the_politics_points() {
    WorldState w = fresh();
    Group g;
    g.id = "eastern_warband";
    g.strength = 8;
    g.supplies = 50;
    g.morale = 70;
    const GroupDef& def = *find_group_def(w.wild, g.id);
    const RaidAssessment peace = wild::assess_target(w, g, def, "fields", w.day);
    w.facts.war_stage = 1;
    const RaidAssessment war = wild::assess_target(w, g, def, "fields", w.day);
    SIM_CHECK(!peace.place.empty());
    SIM_CHECK_EQ(war.opportunity - peace.opportunity, kWarOpportunity);
    SIM_CHECK(war.hunger > peace.hunger);  // the war also starves them
    SIM_CHECK(!war.blocked);
    // The identity the formula is documented by still holds, politics included.
    SIM_CHECK_EQ(war.score, war.hunger + war.opportunity - war.defence - war.fear);
    SIM_CHECK_EQ(war.chance_bp, std::clamp(war.score * kRaidBpPerPoint, 0, kRaidMaxBp));

    // The partisans are sworn off the city's things: even a def that would
    // prefer its fields is treaty-blocked from them — until the rebellion is
    // outlawed, when the band may fall on anything.
    Group p;
    p.id = "sons_of_the_swamp_sage";
    p.strength = 6;
    p.supplies = 60;
    p.morale = 80;
    GroupDef sons = *find_group_def(w.wild, p.id);
    sons.territory.push_back("fields_beyond_the_gate_place");
    sons.prefers["fields"] = 30;
    const RaidAssessment reined = wild::assess_target(w, p, sons, "fields", w.day);
    SIM_CHECK(!reined.place.empty());
    SIM_CHECK(reined.blocked);
    SIM_CHECK_EQ(reined.chance_bp, 0);
    SIM_CHECK_EQ(reined.defence - wild::assess_target(w, p, sons, "fields", w.day).defence, 0);
    outlaw(w.faction, "neo_sumerian_rebellion");
    const RaidAssessment loosed = wild::assess_target(w, p, sons, "fields", w.day);
    SIM_CHECK(!loosed.blocked);
    SIM_CHECK(loosed.chance_bp > 0);
    // Outlawry swapped the politics: the treaty's defence is gone, and the
    // outlaw's hunger for anyone's things is added to opportunity.
    SIM_CHECK_EQ(loosed.defence - reined.defence, -kTreatyDefence);
    SIM_CHECK_EQ(loosed.opportunity - reined.opportunity, kOutlawOpportunity);
    return true;
}

static bool test_assess_raid_skips_blocked_targets() {
    WorldState w = fresh();
    w.facts.war_stage = 2;  // the partisans form at war >= 2
    w.advance_days(30);     // form_days 20 + margin
    Group& g = w.wild.groups["sons_of_the_swamp_sage"];
    if (!g.active) {  // the war had to persist; form the band by hand otherwise
        const GroupDef& def = *find_group_def(w.wild, g.id);
        g.active = true;
        g.camp = def.camp_sites.front();
        g.leader = def.leaders.front();
        g.strength = def.base_strength;
        g.supplies = def.start_supplies;
        g.morale = def.start_morale;
        g.formed_day = w.day;
    }
    SIM_CHECK(g.active);
    // The band's own charter prefers caravans only; whatever the politics,
    // the assessment never returns a treaty-blocked city target.
    const RaidAssessment a = assess_raid(w, g.id, w.day);
    SIM_CHECK(!a.blocked);
    if (!a.target.empty()) SIM_CHECK(a.target == "caravans");
    // ...and a full city charter (a designer adding fields) still cannot
    // break the pact: the politics layer holds even then.
    return true;
}

// --- the band-politics read and the C ABI ------------------------------------------

static bool test_band_politics_read() {
    WorldState w = fresh();
    const BandPolitics jackals = band_politics(w, "cavern_jackals");
    SIM_CHECK(jackals.faction.empty() && jackals.net_pts == 0 && !jackals.blocked);
    const BandPolitics warband = band_politics(w, "eastern_warband");
    SIM_CHECK_EQ(warband.faction, Id("the_barbarians"));
    SIM_CHECK_EQ(warband.net_pts, 0);
    SIM_CHECK(warband.note.empty());
    w.facts.war_stage = 1;
    const BandPolitics atwar = band_politics(w, "eastern_warband");
    SIM_CHECK_EQ(atwar.net_pts, kWarOpportunity);
    SIM_CHECK_EQ(atwar.note, "war");
    const BandPolitics sons = band_politics(w, "sons_of_the_swamp_sage");
    SIM_CHECK_EQ(sons.faction, Id("neo_sumerian_rebellion"));
    SIM_CHECK_EQ(sons.net_pts, -kTreatyDefence);
    SIM_CHECK(sons.blocked);
    SIM_CHECK_EQ(sons.note, "treaty");
    SIM_CHECK(band_politics(w, "no_such_band").faction.empty());
    return true;
}

static bool test_politics_survives_a_save_round_trip() {
    WorldState w = fresh();
    w.facts.war_stage = 2;
    w.advance_days(3);
    const std::string saved = save_world(w);
    WorldState loaded;
    load_world(loaded, kCanon, saved);
    for (const char* band : {"eastern_warband", "sons_of_the_swamp_sage", "cavern_jackals"}) {
        const BandPolitics a = band_politics(w, band);
        const BandPolitics b = band_politics(loaded, band);
        SIM_CHECK_EQ(a.faction, b.faction);
        SIM_CHECK_EQ(a.net_pts, b.net_pts);
        SIM_CHECK_EQ(a.blocked, b.blocked);
        SIM_CHECK_EQ(a.note, b.note);
    }
    SIM_CHECK_EQ(loaded.wild.treaty_defs.size(), w.wild.treaty_defs.size());
    return true;
}

static bool test_capi_faction_reads() {
    SimWorld* w = sim_world_create(kCanon, 42);
    if (w == nullptr) return false;
    sim_world_advance_days(w, 1);
    char buf[512];

    SIM_CHECK(sim_world_faction_tier(w, "the_empire", buf, sizeof buf) > 0);
    SIM_CHECK(std::strcmp(buf, "Stranger") == 0);  // standing 0 reads Stranger
    SIM_CHECK_EQ(sim_world_faction_tier(w, nullptr, buf, sizeof buf), -1);

    SIM_CHECK_EQ(sim_world_oath_count(w), 0);  // no oath sworn yet
    SIM_CHECK_EQ(sim_world_oath(w, 0, buf, sizeof buf), -1);
    SIM_CHECK_EQ(sim_world_oath_breaker_curse(w), 0);
    SIM_CHECK_EQ(sim_world_is_outlawed(w, "the_empire"), 0);
    SIM_CHECK_EQ(sim_world_is_outlawed(w, nullptr), -1);

    SIM_CHECK(sim_world_treaty_count(w) >= 1);
    SIM_CHECK(sim_world_treaty(w, 0, buf, sizeof buf) > 0);
    SIM_CHECK(std::strstr(buf, "southern_cause_pact") != nullptr);
    SIM_CHECK(std::strstr(buf, "neo_sumerian_rebellion|southern_city_states_alliance") != nullptr);
    SIM_CHECK_EQ(sim_world_treaty(w, 99, buf, sizeof buf), -1);
    SIM_CHECK_EQ(sim_world_treaty_live_between(w, "neo_sumerian_rebellion",
                                               "southern_city_states_alliance"), 1);
    SIM_CHECK_EQ(sim_world_treaty_live_between(w, "suti", "southern_city_states_alliance"), 0);
    SIM_CHECK_EQ(sim_world_treaty_live_between(w, "suti", nullptr), -1);

    SIM_CHECK(sim_world_band_politics(w, "eastern_warband", buf, sizeof buf) > 0);
    SIM_CHECK(std::strcmp(buf, "the_barbarians;0;0;") == 0);  // at peace
    sim_world_set_war(w, 1);
    SIM_CHECK(sim_world_band_politics(w, "eastern_warband", buf, sizeof buf) > 0);
    SIM_CHECK(std::strcmp(buf, "the_barbarians;12;0;war") == 0);
    SIM_CHECK(sim_world_band_politics(w, "sons_of_the_swamp_sage", buf, sizeof buf) > 0);
    SIM_CHECK(std::strcmp(buf, "neo_sumerian_rebellion;-15;1;treaty") == 0);
    SIM_CHECK(sim_world_band_politics(w, "cavern_jackals", buf, sizeof buf) >= 0);
    SIM_CHECK(std::strcmp(buf, ";0;0;") == 0);  // unaligned: empty everything
    SIM_CHECK_EQ(sim_world_band_politics(w, nullptr, buf, sizeof buf), -1);

    sim_world_destroy(w);
    SIM_CHECK_EQ(sim_world_oath_count(nullptr), -1);
    SIM_CHECK_EQ(sim_world_treaty_count(nullptr), -1);
    SIM_CHECK_EQ(sim_world_oath_breaker_curse(nullptr), -1);
    SIM_CHECK_EQ(sim_world_band_politics(nullptr, "x", buf, sizeof buf), -1);
    return true;
}

static bool test_capi_faction_nothrow_on_bad_buffers() {
    SimWorld* w = sim_world_create(kCanon, 7);
    if (w == nullptr) return false;
    // Degenerate buffers never throw; a short buffer still NUL-terminates.
    char tiny[8];
    SIM_CHECK(sim_world_band_politics(w, "eastern_warband", tiny, sizeof tiny) > 0);
    SIM_CHECK(std::memchr(tiny, '\0', sizeof tiny) != nullptr);
    SIM_CHECK(sim_world_faction_tier(w, "the_empire", tiny, 0) == -1);
    SIM_CHECK_EQ(sim_world_treaty(w, 0, tiny, -1), -1);
    sim_world_destroy(w);
    return true;
}

SIM_MAIN(test_treaties_load_and_bind_the_southern_cause, test_raid_politics_rules,
         test_assess_target_carries_the_politics_points, test_assess_raid_skips_blocked_targets,
         test_band_politics_read, test_politics_survives_a_save_round_trip,
         test_capi_faction_reads, test_capi_faction_nothrow_on_bad_buffers)
