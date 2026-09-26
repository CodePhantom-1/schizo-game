// Snapshot.cpp — the save file. See sim/Snapshot.hpp for the format overview.
#include "sim/Snapshot.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sim {
namespace {

std::string esc(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += c;
        }
    }
    return out;
}

std::string unesc(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            switch (n) {
                case '\\': out += '\\'; break;
                case 't': out += '\t'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                default: out += n;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::string s64(std::int64_t v) { return std::to_string(v); }
std::string u64(std::uint64_t v) { return std::to_string(v); }
std::string sbool(bool v) { return v ? "1" : "0"; }

class Writer {
public:
    void line(const std::vector<std::string>& fields) {
        bool first = true;
        for (const std::string& f : fields) {
            if (!first) os_ << '\t';
            first = false;
            os_ << esc(f);
        }
        os_ << '\n';
    }
    void raw(const std::string& s) { os_ << s << '\n'; }
    std::string str() const { return os_.str(); }

private:
    std::ostringstream os_;
};

class Reader {
public:
    explicit Reader(const std::string& data) : in_(data) {}

    std::vector<std::string> next() {
        std::string line;
        if (!std::getline(in_, line))
            throw std::runtime_error("snapshot: unexpected end of data");
        std::vector<std::string> raw = split_tab(line);
        std::vector<std::string> out;
        out.reserve(raw.size());
        for (const std::string& f : raw) out.push_back(unesc(f));
        return out;
    }

    // "<TAG>\t<count>" — returns count.
    std::size_t section(const std::string& tag) {
        std::vector<std::string> f = next();
        if (f.size() != 2 || f[0] != tag)
            throw std::runtime_error("snapshot: expected section " + tag);
        return static_cast<std::size_t>(parse_u64(f[1]));
    }

    // "<TAG>\t<value>" — returns the raw value.
    std::string scalar(const std::string& tag) {
        std::vector<std::string> f = next();
        if (f.size() != 2 || f[0] != tag)
            throw std::runtime_error("snapshot: expected field " + tag);
        return f[1];
    }


    // True once every line has been read (only trailing whitespace remains).
    bool at_end() {
        in_ >> std::ws;
        return in_.peek() == std::char_traits<char>::eof();
    }

    static std::int64_t parse_i64(const std::string& s) {
        try {
            std::size_t pos = 0;
            std::int64_t v = std::stoll(s, &pos);
            if (pos != s.size()) throw std::runtime_error("trailing");
            return v;
        } catch (...) {
            throw std::runtime_error("snapshot: bad integer '" + s + "'");
        }
    }
    static std::uint64_t parse_u64(const std::string& s) {
        try {
            std::size_t pos = 0;
            std::uint64_t v = std::stoull(s, &pos);
            if (pos != s.size()) throw std::runtime_error("trailing");
            return v;
        } catch (...) {
            throw std::runtime_error("snapshot: bad integer '" + s + "'");
        }
    }
    // W4-A: the tag (first field) of the next line without consuming it; ""
    // at the end of the data. Lets an optional trailing section be detected
    // by name, whatever other optional sections precede or follow it.
    std::string peek_tag() {
        in_ >> std::ws;
        const std::streampos pos = in_.tellg();
        std::string line;
        if (!std::getline(in_, line)) {
            in_.clear();
            return {};
        }
        in_.seekg(pos);
        const std::size_t tab = line.find('\t');
        return unesc(tab == std::string::npos ? line : line.substr(0, tab));
    }
    // (W4-B uses W4-A's peek_tag above for its COMBAT_* sections.)

    static bool parse_bool(const std::string& s) {
        if (s == "1") return true;
        if (s == "0") return false;
        throw std::runtime_error("snapshot: bad bool '" + s + "'");
    }

private:
    std::istringstream in_;
};

// P0a: one item stack as 8 fields (item, qty, quality, condition, owner,
// stolen, made_day, bound) — inventories, world items and containers share it.
std::vector<std::string> stack_fields(const ItemStack& s) {
    return {s.item, s64(s.qty), s64(s.quality), s64(s.condition), s.owner,
            sbool(s.stolen), s64(s.made_day), sbool(s.bound)};
}

ItemStack stack_from(const std::vector<std::string>& f, std::size_t at, const char* what) {
    if (f.size() < at + 8) throw std::runtime_error(std::string("snapshot: bad ") + what + " stack row");
    ItemStack s;
    s.item = f[at];
    s.qty = static_cast<int>(Reader::parse_i64(f[at + 1]));
    s.quality = static_cast<int>(Reader::parse_i64(f[at + 2]));
    s.condition = static_cast<int>(Reader::parse_i64(f[at + 3]));
    s.owner = f[at + 4];
    s.stolen = Reader::parse_bool(f[at + 5]);
    s.made_day = Reader::parse_i64(f[at + 6]);
    s.bound = Reader::parse_bool(f[at + 7]);
    return s;
}

}  // namespace

std::string save_world(const WorldState& w) {
    Writer wr;
    wr.raw("SIMSAVE 2");
    wr.line({"day", s64(w.day)});
    wr.line({"seed", u64(w.seed)});
    wr.line({"rng", u64(w.rng.state())});
    wr.line({"drought_stage", s64(w.facts.drought_stage)});
    wr.line({"war_stage", s64(w.facts.war_stage)});

    // ECONOMY
    {
        std::int64_t n = 0;
        for (const auto& [city, book] : w.economy.market_by_city)
            n += static_cast<std::int64_t>(book.silver_by_item.size());
        wr.line({"ECONOMY_MARKETS", s64(n)});
        for (const auto& [city, book] : w.economy.market_by_city)
            for (const auto& [item, price] : book.silver_by_item)
                wr.line({city, item, s64(price)});

        wr.line({"ECONOMY_STOCK", s64(static_cast<std::int64_t>(w.economy.stock_by_city_item.size()))});
        for (const auto& [key, qty] : w.economy.stock_by_city_item)
            wr.line({key, s64(qty)});
    }

    // POPULATION
    {
        wr.line({"POP_NPCS", s64(static_cast<std::int64_t>(w.population.npcs.size()))});
        for (const Npc& n : w.population.npcs) {
            wr.line({n.id, n.name, n.home_city, n.household, n.faction_id, n.patron_deity,
                     s64(n.loyalty), s64(static_cast<std::int64_t>(n.memory.size())), n.role});
            for (const MemoryEntry& m : n.memory)
                wr.line({s64(m.day), m.subject, m.fact});
        }

        std::int64_t edges = 0;
        for (const auto& [npc_id, list] : w.population.knows)
            edges += static_cast<std::int64_t>(list.size());
        wr.line({"POP_KNOWS", s64(edges)});
        for (const auto& [npc_id, list] : w.population.knows)
            for (const Id& known : list)
                wr.line({npc_id, known});
    }

    // FACTION
    {
        wr.line({"FACTION_STANDING", s64(static_cast<std::int64_t>(w.faction.standing_by_faction.size()))});
        for (const auto& [fid, val] : w.faction.standing_by_faction)
            wr.line({fid, s64(val)});

        wr.line({"FACTION_OATHS", s64(static_cast<std::int64_t>(w.faction.oaths.size()))});
        for (const Oath& o : w.faction.oaths)
            wr.line({o.id, o.swearer, o.to_faction, s64(o.day), sbool(o.broken)});

        wr.line({"FACTION_CURSE", sbool(w.faction.oath_breaker_curse)});

        wr.line({"FACTION_OUTLAWED", s64(static_cast<std::int64_t>(w.faction.outlawed_by_faction.size()))});
        for (const auto& [fid, val] : w.faction.outlawed_by_faction)
            wr.line({fid, sbool(val)});
    }

    // MAGIC
    {
        wr.line({"MAGIC_FAVOUR", s64(static_cast<std::int64_t>(w.magic.favour_by_deity.size()))});
        for (const auto& [deity, val] : w.magic.favour_by_deity)
            wr.line({deity, s64(val)});
        wr.line({"MAGIC_PURITY", s64(w.magic.purity)});
        wr.line({"MAGIC_PLACE", w.magic.place});
    }

    // JUSTICE
    {
        wr.line({"JUSTICE_OPEN", s64(static_cast<std::int64_t>(w.justice.open_crimes.size()))});
        for (const Crime& c : w.justice.open_crimes)
            wr.line({c.id, c.criminal, c.law_row, c.crime_kind, s64(c.day), c.witnessed_by,
                     sbool(c.atoned), c.stage, s64(c.hearing_day), c.victim, c.place_city});

        wr.line({"JUSTICE_VERDICTS", s64(static_cast<std::int64_t>(w.justice.verdicts.size()))});
        for (const Hearing& h : w.justice.verdicts)
            wr.line({h.crime_id, s64(h.day), h.verdict, h.tablet_id, s64(h.compensation_paid)});

        wr.line({"JUSTICE_NEXT_ID", s64(w.justice.next_id)});
    }

    // EVENTS
    {
        wr.line({"EVENTS_RULES", s64(static_cast<std::int64_t>(w.events.rules.size()))});
        for (const EventRule& r : w.events.rules) {
            std::vector<std::string> fields = {r.id, r.category, sbool(r.repeatable),
                                               s64(static_cast<std::int64_t>(r.triggers.size()))};
            for (const std::string& t : r.triggers) fields.push_back(t);
            wr.line(fields);
        }

        wr.line({"EVENTS_FIRED", s64(static_cast<std::int64_t>(w.events.fired.size()))});
        for (const TriggeredEvent& e : w.events.fired)
            wr.line({e.rule_id, s64(e.day), e.summary});

        wr.line({"EVENTS_FIRE_COUNT", s64(static_cast<std::int64_t>(w.events.fire_count_by_rule.size()))});
        for (const auto& [rule_id, count] : w.events.fire_count_by_rule)
            wr.line({rule_id, s64(count)});
    }

    // PROPERTY
    {
        wr.line({"PROPERTY_ASSETS", s64(static_cast<std::int64_t>(w.property.assets.size()))});
        for (const Asset& a : w.property.assets)
            wr.line({a.id, a.kind, a.owner, a.deed_tablet_id, s64(a.income_per_day), a.city});

        wr.line({"PROPERTY_LOANS", s64(static_cast<std::int64_t>(w.property.loans.size()))});
        for (const Loan& l : w.property.loans)
            wr.line({l.id, l.debtor, l.creditor, s64(l.principal), s64(l.rate_pct), s64(l.issued),
                     s64(l.due), sbool(l.repaid), s64(l.accrued)});

        wr.line({"PROPERTY_PURSE", s64(static_cast<std::int64_t>(w.property.purse_by_owner.size()))});
        for (const auto& [owner, amount] : w.property.purse_by_owner)
            wr.line({owner, s64(amount)});

        wr.line({"PROPERTY_REPORTS", s64(static_cast<std::int64_t>(w.property.steward_reports.size()))});
        for (const std::string& rep : w.property.steward_reports)
            wr.line({rep});

        wr.line({"PROPERTY_NEXT_ID", s64(w.property.next_id)});
    }

    // QUESTS
    {
        wr.line({"QUESTS_DEFS", s64(static_cast<std::int64_t>(w.quests.defs.size()))});
        for (const QuestDef& d : w.quests.defs)
            wr.line({d.id, d.kind, s64(d.deadline_days), s64(d.reward_silver), d.reward_faction,
                     s64(d.reward_standing)});

        wr.line({"QUESTS_ACTIVE", s64(static_cast<std::int64_t>(w.quests.active.size()))});
        for (const Quest& q : w.quests.active)
            wr.line({q.def_id, s64(q.accepted), s64(q.deadline), sbool(q.failed), q.stage});

        wr.line({"QUESTS_COMPLETED", s64(static_cast<std::int64_t>(w.quests.completed.size()))});
        for (const Id& id : w.quests.completed)
            wr.line({id});

        wr.line({"QUESTS_FAILED", s64(static_cast<std::int64_t>(w.quests.failed_list.size()))});
        for (const Id& id : w.quests.failed_list)
            wr.line({id});

        std::int64_t entries = 0;
        for (const auto& [qid, list] : w.quests.journal) entries += static_cast<std::int64_t>(list.size());
        wr.line({"QUESTS_JOURNAL", s64(entries)});
        for (const auto& [qid, list] : w.quests.journal)
            for (const JournalEntry& e : list) wr.line({qid, s64(e.day), e.stage, e.text});
    }

    // NEEDS (W2-I; SIMSAVE 2)
    {
        wr.line({"NEEDS", s64(static_cast<std::int64_t>(w.needs.by_actor.size()))});
        for (const auto& [actor, n] : w.needs.by_actor)
            wr.line({actor, s64(n.hunger), s64(n.thirst), s64(n.fatigue)});
    }

    // INVENTORIES (W2-I; P0a: one row per stack, 8 fields — stack_fields)
    {
        wr.line({"INVENTORIES", s64(static_cast<std::int64_t>(w.inventories.size()))});
        for (const auto& [actor, inv] : w.inventories) {
            wr.line({actor, s64(static_cast<std::int64_t>(inv.stacks.size()))});
            for (const ItemStack& s : inv.stacks) wr.line(stack_fields(s));
        }
    }

    // K-1 RITES (additive trailing sections; optional on load so a save
    // written before K-1 still loads): rite knowledge, wards, omens.
    {
        wr.line({"MAGIC_KNOWN", s64(static_cast<std::int64_t>(w.magic.known_rites.size()))});
        for (const Id& rite : w.magic.known_rites) wr.line({rite});

        wr.line({"RITE_WARDS", s64(static_cast<std::int64_t>(w.rite_effects.wards_by_place.size()))});
        for (const auto& [place, ward] : w.rite_effects.wards_by_place)
            wr.line({place, ward.rite_id, s64(ward.laid), s64(ward.until)});

        wr.line({"RITE_OMENS", s64(static_cast<std::int64_t>(w.rite_effects.omens.size()))});
        for (const Omen& o : w.rite_effects.omens)
            wr.line({s64(o.day), o.rite_id, o.subject, o.sign, s64(o.confidence_pct)});
    }

    // W4-A CHARACTER (additive trailing sections, detected by tag on load so
    // a save written before W4-A still loads): the player's sheet and the
    // npcs' light sheets. The derived bonuses are not saved — they are
    // rebuilt from the talents and callings against the canon catalog.
    {
        const CharacterState& c = w.character;
        wr.line({"CHAR_CORE", s64(c.level), s64(c.xp), s64(c.talent_points), c.calling,
                 c.specialisation, c.second_calling});
        wr.line({"CHAR_ATTRS", s64(static_cast<std::int64_t>(c.attributes.size()))});
        for (const auto& [attr, base] : c.attributes) {
            const auto ex = c.attr_exercise.find(attr);
            wr.line({attr, s64(base), s64(ex == c.attr_exercise.end() ? 0 : ex->second)});
        }
        wr.line({"CHAR_SKILLS", s64(static_cast<std::int64_t>(c.skills.size()))});
        for (const auto& [skill, sp] : c.skills) wr.line({skill, s64(sp.value), s64(sp.progress)});
        wr.line({"CHAR_TALENTS", s64(static_cast<std::int64_t>(c.talents.size()))});
        for (const Id& t : c.talents) wr.line({t});
        wr.line({"CHAR_RANKS", s64(static_cast<std::int64_t>(c.rank_by_polity.size()))});
        for (const auto& [polity, tier] : c.rank_by_polity) wr.line({polity, s64(tier)});
        wr.line({"CHAR_NPCS", s64(static_cast<std::int64_t>(w.npc_sheets.size()))});
        for (const auto& [npc, sheet] : w.npc_sheets) {
            wr.line({npc, s64(static_cast<std::int64_t>(sheet.attributes.size())),
                     s64(static_cast<std::int64_t>(sheet.skills.size()))});
            for (const auto& [attr, v] : sheet.attributes) wr.line({attr, s64(v)});
            for (const auto& [skill, v] : sheet.skills) wr.line({skill, s64(v)});
        }
    }
    // --- W4-C WILD (trailing optional section; sim/Wild.hpp wild_save_rows).
    {
        const auto rows = wild_save_rows(w.wild);
        wr.line({"WILD", s64(static_cast<std::int64_t>(rows.size()))});
        for (const auto& row : rows) wr.line(row);
    }
    // --- end W4-C

    // W4-B COMBAT (additive trailing sections; optional on load, recognised
    // by tag, so a save written before W4-B still loads).
    {
        wr.line({"COMBAT_ACTORS", s64(static_cast<std::int64_t>(w.combat.by_actor.size()))});
        for (const auto& [id, c] : w.combat.by_actor) {
            wr.line({id, s64(c.health), s64(c.stamina), s64(c.morale), s64(c.ko_hours), sbool(c.dead),
                     sbool(c.outlaw), s64(c.rest_hours), c.style, c.surrendered_to, c.captor,
                     c.last_attacker, c.weapon, c.shield, s64(static_cast<std::int64_t>(c.armour.size())),
                     s64(static_cast<std::int64_t>(c.wounds.size())),
                     s64(static_cast<std::int64_t>(c.lasting.size())),
                     s64(static_cast<std::int64_t>(c.durability.size()))});
            for (const Id& a : c.armour) wr.line({a});
            for (const Wound& wd : c.wounds)
                wr.line({s64(wd.zone), wd.type, s64(wd.severity), s64(wd.damage), s64(wd.remaining),
                         s64(wd.bleed), s64(wd.clot_hours), s64(wd.treatment), s64(wd.day)});
            for (const std::string& m : c.lasting) wr.line({m});
            for (const auto& [item, dur] : c.durability) wr.line({item, s64(dur)});
        }
        wr.line({"COMBAT_HOSTILITY", s64(static_cast<std::int64_t>(w.combat.hostilities.size()))});
        for (const Hostility& h : w.combat.hostilities) wr.line({h.aggressor, h.other, s64(h.day)});
        wr.line({"COMBAT_DUELS", s64(static_cast<std::int64_t>(w.combat.duels.size()))});
        for (const Duel& d : w.combat.duels) wr.line({d.a, d.b, s64(d.day)});
        wr.line({"COMBAT_PRISONERS", s64(static_cast<std::int64_t>(w.combat.prisoners.size()))});
        for (const Prisoner& p : w.combat.prisoners) wr.line({p.captive, p.captor, s64(p.day)});
        wr.line({"COMBAT_DEATHS", s64(static_cast<std::int64_t>(w.combat.deaths.size()))});
        for (const DeathRecord& d : w.combat.deaths) wr.line({d.id, s64(d.day), d.killer, d.cause});
        wr.line({"COMBAT_SEQ", u64(w.combat.seq)});
    }

    // --- W5 DIVINE (additive trailing sections; optional on load, recognised
    // by tag, so a save written before W5 still loads: no wrath, no curse).
    // Self-contained block — keep it delimited if you edit nearby.
    {
        std::int64_t entries = 0;
        for (const auto& [offender, per_deity] : w.divine.wrath_by_offender)
            entries += static_cast<std::int64_t>(per_deity.size());
        wr.line({"DIVINE_WRATH", s64(entries)});
        for (const auto& [offender, per_deity] : w.divine.wrath_by_offender)
            for (const auto& [deity, value] : per_deity)
                wr.line({offender, deity, s64(value)});

        wr.line({"DIVINE_CURSE", s64(static_cast<std::int64_t>(w.divine.curse_by_offender.size()))});
        for (const auto& [offender, deity] : w.divine.curse_by_offender)
            wr.line({offender, deity});

        wr.line({"DIVINE_PENDING", s64(static_cast<std::int64_t>(w.divine.pending_favour_penalty.size()))});
        for (const auto& [deity, delta] : w.divine.pending_favour_penalty)
            wr.line({deity, s64(delta)});

        wr.line({"DIVINE_DECAY_ACC", s64(w.divine.decay_acc)});
    }
    // --- end W5

    // --- P0a: goods in the world and in containers (optional trailing
    // sections; a save written before P0a loads with nothing on the ground).
    {
        wr.line({"WORLD_ITEMS_NEXT", s64(w.world_items.next_id)});
        wr.line({"WORLD_ITEMS", s64(static_cast<std::int64_t>(w.world_items.items.size()))});
        for (const WorldItem& wi : w.world_items.items) {
            std::vector<std::string> f = {wi.id, wi.place, s64(wi.x_cm), s64(wi.y_cm), s64(wi.z_cm),
                                          s64(wi.dropped_day), wi.dropped_by};
            for (std::string& s : stack_fields(wi.stack)) f.push_back(std::move(s));
            wr.line(f);
        }
        wr.line({"CONTAINERS", s64(static_cast<std::int64_t>(w.world_items.containers.size()))});
        for (const auto& [id, c] : w.world_items.containers) {
            wr.line({c.id, c.place, c.kind, c.owner, sbool(c.locked), c.key_item, s64(c.capacity_g),
                     s64(static_cast<std::int64_t>(c.contents.stacks.size()))});
            for (const ItemStack& s : c.contents.stacks) wr.line(stack_fields(s));
        }
    }
    // --- end P0a

    return wr.str();
}

void load_world(WorldState& out, const std::string& canon_dir, const std::string& data) {
    try {
        // Parsed into a local so a truncated/corrupt save throws with the
        // caller's live world (`out`) left byte-for-byte untouched; `out` is
        // only overwritten by the std::move below, once every section has
        // parsed successfully.
        WorldState w;
        Reader rd(data);

        std::vector<std::string> header = rd.next();
        if (header.size() == 1 && header[0] == "SIMSAVE 1")
            throw std::runtime_error("snapshot: SIMSAVE 1 is a pre-wave-2 save (no longer supported)");
        if (header.size() != 1 || header[0] != "SIMSAVE 2")
            throw std::runtime_error("snapshot: unknown or missing version header");

        std::int64_t day = Reader::parse_i64(rd.scalar("day"));
        std::uint64_t seed = Reader::parse_u64(rd.scalar("seed"));
        std::uint64_t rng_state = Reader::parse_u64(rd.scalar("rng"));
        std::int64_t drought = Reader::parse_i64(rd.scalar("drought_stage"));
        std::int64_t war = Reader::parse_i64(rd.scalar("war_stage"));

        // Rebuilds db + calendar deterministically from canon, and resets
        // every module state; everything below replaces that fresh state.
        w.init(canon_dir, seed);
        w.day = day;
        w.rng.set_state(rng_state);
        w.facts.drought_stage = static_cast<int>(drought);
        w.facts.war_stage = static_cast<int>(war);

        // ECONOMY
        w.economy.market_by_city.clear();
        w.economy.stock_by_city_item.clear();
        {
            std::size_t n = rd.section("ECONOMY_MARKETS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 3) throw std::runtime_error("snapshot: bad economy market row");
                w.economy.market_by_city[f[0]].silver_by_item[f[1]] = Reader::parse_i64(f[2]);
            }
            n = rd.section("ECONOMY_STOCK");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad economy stock row");
                w.economy.stock_by_city_item[f[0]] = Reader::parse_i64(f[1]);
            }
        }

        // POPULATION
        w.population.npcs.clear();
        w.population.knows.clear();
        {
            std::size_t n = rd.section("POP_NPCS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 9) throw std::runtime_error("snapshot: bad npc row");
                Npc npc;
                npc.id = f[0];
                npc.name = f[1];
                npc.home_city = f[2];
                npc.household = f[3];
                npc.faction_id = f[4];
                npc.patron_deity = f[5];
                npc.loyalty = static_cast<int>(Reader::parse_i64(f[6]));
                npc.role = f[8];
                std::size_t memcount = static_cast<std::size_t>(Reader::parse_u64(f[7]));
                // No reserve(memcount): the count is untrusted file data; a
                // corrupt one must fail as "unexpected end of data", not as a
                // petabyte allocation.
                for (std::size_t j = 0; j < memcount; ++j) {
                    std::vector<std::string> mf = rd.next();
                    if (mf.size() != 3) throw std::runtime_error("snapshot: bad memory row");
                    MemoryEntry m;
                    m.day = Reader::parse_i64(mf[0]);
                    m.subject = mf[1];
                    m.fact = mf[2];
                    npc.memory.push_back(std::move(m));
                }
                w.population.npcs.push_back(std::move(npc));
            }
            n = rd.section("POP_KNOWS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad knows row");
                w.population.knows[f[0]].push_back(f[1]);
            }
        }

        // FACTION
        w.faction.standing_by_faction.clear();
        w.faction.oaths.clear();
        {
            std::size_t n = rd.section("FACTION_STANDING");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad standing row");
                w.faction.standing_by_faction[f[0]] = static_cast<int>(Reader::parse_i64(f[1]));
            }
            n = rd.section("FACTION_OATHS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 5) throw std::runtime_error("snapshot: bad oath row");
                Oath o;
                o.id = f[0];
                o.swearer = f[1];
                o.to_faction = f[2];
                o.day = Reader::parse_i64(f[3]);
                o.broken = Reader::parse_bool(f[4]);
                w.faction.oaths.push_back(std::move(o));
            }
            w.faction.oath_breaker_curse = Reader::parse_bool(rd.scalar("FACTION_CURSE"));
            w.faction.outlawed_by_faction.clear();
            n = rd.section("FACTION_OUTLAWED");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad outlawed row");
                w.faction.outlawed_by_faction[f[0]] = Reader::parse_bool(f[1]);
            }
        }

        // MAGIC
        w.magic.favour_by_deity.clear();
        {
            std::size_t n = rd.section("MAGIC_FAVOUR");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad favour row");
                w.magic.favour_by_deity[f[0]] = static_cast<int>(Reader::parse_i64(f[1]));
            }
            w.magic.purity = static_cast<int>(Reader::parse_i64(rd.scalar("MAGIC_PURITY")));
            w.magic.place = rd.scalar("MAGIC_PLACE");
        }

        // JUSTICE
        w.justice.open_crimes.clear();
        w.justice.verdicts.clear();
        {
            std::size_t n = rd.section("JUSTICE_OPEN");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 11) throw std::runtime_error("snapshot: bad crime row");
                Crime c;
                c.id = f[0];
                c.criminal = f[1];
                c.law_row = f[2];
                c.crime_kind = f[3];
                c.day = Reader::parse_i64(f[4]);
                c.witnessed_by = f[5];
                c.atoned = Reader::parse_bool(f[6]);
                c.stage = f[7];
                c.hearing_day = Reader::parse_i64(f[8]);
                c.victim = f[9];
                c.place_city = f[10];
                w.justice.open_crimes.push_back(std::move(c));
            }
            n = rd.section("JUSTICE_VERDICTS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 5) throw std::runtime_error("snapshot: bad hearing row");
                Hearing h;
                h.crime_id = f[0];
                h.day = Reader::parse_i64(f[1]);
                h.verdict = f[2];
                h.tablet_id = f[3];
                h.compensation_paid = Reader::parse_i64(f[4]);
                w.justice.verdicts.push_back(std::move(h));
            }
            w.justice.next_id = static_cast<int>(Reader::parse_i64(rd.scalar("JUSTICE_NEXT_ID")));
        }

        // EVENTS
        w.events.rules.clear();
        w.events.fired.clear();
        w.events.fire_count_by_rule.clear();
        {
            std::size_t n = rd.section("EVENTS_RULES");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() < 4) throw std::runtime_error("snapshot: bad event rule row");
                EventRule r;
                r.id = f[0];
                r.category = f[1];
                r.repeatable = Reader::parse_bool(f[2]);
                std::size_t tcount = static_cast<std::size_t>(Reader::parse_u64(f[3]));
                if (f.size() != 4 + tcount) throw std::runtime_error("snapshot: bad event rule triggers");
                for (std::size_t j = 0; j < tcount; ++j) r.triggers.push_back(f[4 + j]);
                w.events.rules.push_back(std::move(r));
            }
            n = rd.section("EVENTS_FIRED");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 3) throw std::runtime_error("snapshot: bad fired-event row");
                TriggeredEvent e;
                e.rule_id = f[0];
                e.day = Reader::parse_i64(f[1]);
                e.summary = f[2];
                w.events.fired.push_back(std::move(e));
            }
            n = rd.section("EVENTS_FIRE_COUNT");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad fire-count row");
                w.events.fire_count_by_rule[f[0]] = static_cast<int>(Reader::parse_i64(f[1]));
            }
        }

        // PROPERTY
        w.property.assets.clear();
        w.property.loans.clear();
        w.property.purse_by_owner.clear();
        w.property.steward_reports.clear();
        {
            std::size_t n = rd.section("PROPERTY_ASSETS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 6) throw std::runtime_error("snapshot: bad asset row");
                Asset a;
                a.id = f[0];
                a.kind = f[1];
                a.owner = f[2];
                a.deed_tablet_id = f[3];
                a.income_per_day = Reader::parse_i64(f[4]);
                a.city = f[5];
                w.property.assets.push_back(std::move(a));
            }
            n = rd.section("PROPERTY_LOANS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 9) throw std::runtime_error("snapshot: bad loan row");
                Loan l;
                l.id = f[0];
                l.debtor = f[1];
                l.creditor = f[2];
                l.principal = Reader::parse_i64(f[3]);
                l.rate_pct = static_cast<int>(Reader::parse_i64(f[4]));
                l.issued = Reader::parse_i64(f[5]);
                l.due = Reader::parse_i64(f[6]);
                l.repaid = Reader::parse_bool(f[7]);
                l.accrued = Reader::parse_i64(f[8]);
                w.property.loans.push_back(std::move(l));
            }
            n = rd.section("PROPERTY_PURSE");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad purse row");
                w.property.purse_by_owner[f[0]] = Reader::parse_i64(f[1]);
            }
            n = rd.section("PROPERTY_REPORTS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 1) throw std::runtime_error("snapshot: bad steward report row");
                w.property.steward_reports.push_back(f[0]);
            }
            w.property.next_id = static_cast<int>(Reader::parse_i64(rd.scalar("PROPERTY_NEXT_ID")));
        }

        // QUESTS
        w.quests.defs.clear();
        w.quests.active.clear();
        w.quests.completed.clear();
        w.quests.failed_list.clear();
        {
            std::size_t n = rd.section("QUESTS_DEFS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 6) throw std::runtime_error("snapshot: bad quest def row");
                QuestDef d;
                d.id = f[0];
                d.kind = f[1];
                d.deadline_days = static_cast<int>(Reader::parse_i64(f[2]));
                d.reward_silver = Reader::parse_i64(f[3]);
                d.reward_faction = f[4];
                d.reward_standing = static_cast<int>(Reader::parse_i64(f[5]));
                w.quests.defs.push_back(std::move(d));
            }
            n = rd.section("QUESTS_ACTIVE");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 5) throw std::runtime_error("snapshot: bad active quest row");
                Quest q;
                q.def_id = f[0];
                q.accepted = Reader::parse_i64(f[1]);
                q.deadline = Reader::parse_i64(f[2]);
                q.failed = Reader::parse_bool(f[3]);
                q.stage = f[4];
                w.quests.active.push_back(std::move(q));
            }
            n = rd.section("QUESTS_COMPLETED");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 1) throw std::runtime_error("snapshot: bad completed-quest row");
                w.quests.completed.push_back(f[0]);
            }
            n = rd.section("QUESTS_FAILED");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 1) throw std::runtime_error("snapshot: bad failed-quest row");
                w.quests.failed_list.push_back(f[0]);
            }
            w.quests.journal.clear();
            n = rd.section("QUESTS_JOURNAL");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 4) throw std::runtime_error("snapshot: bad journal row");
                w.quests.journal[f[0]].push_back(JournalEntry{Reader::parse_i64(f[1]), f[2], f[3]});
            }
        }

        // NEEDS
        w.needs.by_actor.clear();
        {
            std::size_t n = rd.section("NEEDS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 4) throw std::runtime_error("snapshot: bad needs row");
                Needs& nd = w.needs.by_actor[f[0]];
                nd.hunger = static_cast<int>(Reader::parse_i64(f[1]));
                nd.thirst = static_cast<int>(Reader::parse_i64(f[2]));
                nd.fatigue = static_cast<int>(Reader::parse_i64(f[3]));
            }
        }

        // INVENTORIES
        w.inventories.clear();
        {
            std::size_t n = rd.section("INVENTORIES");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad inventory header row");
                const Id actor = f[0];
                std::size_t itemcount = static_cast<std::size_t>(Reader::parse_u64(f[1]));
                Inventory& inv = w.inventories[actor];
                for (std::size_t j = 0; j < itemcount; ++j) {
                    std::vector<std::string> itf = rd.next();
                    if (itf.size() == 2) {  // a pre-P0a save: item, count
                        add_items(inv, itf[0], static_cast<int>(Reader::parse_i64(itf[1])));
                        continue;
                    }
                    if (itf.size() != 8) throw std::runtime_error("snapshot: bad inventory item row");
                    add_stack(inv, stack_from(itf, 0, "inventory"));
                }
            }
        }

        // K-1 RITES — absent in a pre-K-1 save (the file ends here): the
        // fresh init() state (nothing known, no ward, no omen) stands.
        w.magic.known_rites.clear();
        w.rite_effects = RiteEffectsState{};
        if (!rd.at_end()) {
            std::size_t n = rd.section("MAGIC_KNOWN");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 1) throw std::runtime_error("snapshot: bad known-rite row");
                w.magic.known_rites.insert(f[0]);
            }
            n = rd.section("RITE_WARDS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 4) throw std::runtime_error("snapshot: bad ward row");
                Ward& wd = w.rite_effects.wards_by_place[f[0]];
                wd.place = f[0];
                wd.rite_id = f[1];
                wd.laid = Reader::parse_i64(f[2]);
                wd.until = Reader::parse_i64(f[3]);
            }
            n = rd.section("RITE_OMENS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 5) throw std::runtime_error("snapshot: bad omen row");
                Omen o;
                o.day = Reader::parse_i64(f[0]);
                o.rite_id = f[1];
                o.subject = f[2];
                o.sign = f[3];
                o.confidence_pct = static_cast<int>(Reader::parse_i64(f[4]));
                w.rite_effects.omens.push_back(std::move(o));
            }
        }

        // W4-A CHARACTER — absent in a pre-W4-A save: the fresh init() sheets
        // (a level-1 prisoner; residents seeded from their roles) stand.
        if (rd.peek_tag() == "CHAR_CORE") {
            std::vector<std::string> f = rd.next();
            if (f.size() != 7) throw std::runtime_error("snapshot: bad character core row");
            CharacterState c = new_character(w.progression);
            c.level = static_cast<int>(Reader::parse_i64(f[1]));
            c.xp = static_cast<int>(Reader::parse_i64(f[2]));
            c.talent_points = static_cast<int>(Reader::parse_i64(f[3]));
            c.calling = f[4];
            c.specialisation = f[5];
            c.second_calling = f[6];
            std::size_t n = rd.section("CHAR_ATTRS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> r = rd.next();
                if (r.size() != 3) throw std::runtime_error("snapshot: bad character attribute row");
                c.attributes[r[0]] = static_cast<int>(Reader::parse_i64(r[1]));
                const int ex = static_cast<int>(Reader::parse_i64(r[2]));
                if (ex != 0) c.attr_exercise[r[0]] = ex;
            }
            n = rd.section("CHAR_SKILLS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> r = rd.next();
                if (r.size() != 3) throw std::runtime_error("snapshot: bad character skill row");
                SkillProgress& sp = c.skills[r[0]];
                sp.value = static_cast<int>(Reader::parse_i64(r[1]));
                sp.progress = Reader::parse_i64(r[2]);
            }
            n = rd.section("CHAR_TALENTS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> r = rd.next();
                if (r.size() != 1) throw std::runtime_error("snapshot: bad character talent row");
                c.talents.push_back(r[0]);
            }
            n = rd.section("CHAR_RANKS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> r = rd.next();
                if (r.size() != 2) throw std::runtime_error("snapshot: bad character rank row");
                c.rank_by_polity[r[0]] = static_cast<int>(Reader::parse_i64(r[1]));
            }
            refresh_derived(w.progression, c);
            w.character = std::move(c);

            w.npc_sheets.clear();
            n = rd.section("CHAR_NPCS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> r = rd.next();
                if (r.size() != 3) throw std::runtime_error("snapshot: bad npc sheet row");
                NpcSheet& sheet = w.npc_sheets[r[0]];
                const std::size_t na = static_cast<std::size_t>(Reader::parse_u64(r[1]));
                const std::size_t ns = static_cast<std::size_t>(Reader::parse_u64(r[2]));
                for (std::size_t j = 0; j < na; ++j) {
                    std::vector<std::string> a = rd.next();
                    if (a.size() != 2) throw std::runtime_error("snapshot: bad npc sheet attribute row");
                    sheet.attributes[a[0]] = static_cast<int>(Reader::parse_i64(a[1]));
                }
                for (std::size_t j = 0; j < ns; ++j) {
                    std::vector<std::string> k = rd.next();
                    if (k.size() != 2) throw std::runtime_error("snapshot: bad npc sheet skill row");
                    sheet.skills[k[0]] = static_cast<int>(Reader::parse_i64(k[1]));
                }
            }
        }
        // --- W4-C WILD — optional: absent in a pre-W4-C save, where the fresh
        // init_wild state (day-1 camps, full herds, unrobbed sites) stands.
        if (!rd.at_end() && rd.peek_tag() == "WILD") {
            const std::size_t n = rd.section("WILD");
            std::vector<std::vector<std::string>> rows;
            for (std::size_t i = 0; i < n; ++i) rows.push_back(rd.next());
            wild_load_rows(w.wild, rows);
        }
        // --- end W4-C

        // W4-B COMBAT — absent in a pre-W4-B save: nobody is hurt.
        w.combat = CombatState{};
        if (rd.peek_tag() == "COMBAT_ACTORS") {
            std::size_t n = rd.section("COMBAT_ACTORS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 18) throw std::runtime_error("snapshot: bad combatant row");
                Combatant c;
                c.id = f[0];
                c.health = static_cast<int>(Reader::parse_i64(f[1]));
                c.stamina = static_cast<int>(Reader::parse_i64(f[2]));
                c.morale = static_cast<int>(Reader::parse_i64(f[3]));
                c.ko_hours = static_cast<int>(Reader::parse_i64(f[4]));
                c.dead = Reader::parse_bool(f[5]);
                c.outlaw = Reader::parse_bool(f[6]);
                c.rest_hours = static_cast<int>(Reader::parse_i64(f[7]));
                c.style = f[8];
                c.surrendered_to = f[9];
                c.captor = f[10];
                c.last_attacker = f[11];
                c.weapon = f[12];
                c.shield = f[13];
                const std::size_t na = static_cast<std::size_t>(Reader::parse_u64(f[14]));
                const std::size_t nw = static_cast<std::size_t>(Reader::parse_u64(f[15]));
                const std::size_t nl = static_cast<std::size_t>(Reader::parse_u64(f[16]));
                const std::size_t nd = static_cast<std::size_t>(Reader::parse_u64(f[17]));
                for (std::size_t j = 0; j < na; ++j) {
                    std::vector<std::string> g = rd.next();
                    if (g.size() != 1) throw std::runtime_error("snapshot: bad armour row");
                    c.armour.push_back(g[0]);
                }
                for (std::size_t j = 0; j < nw; ++j) {
                    std::vector<std::string> g = rd.next();
                    if (g.size() != 9) throw std::runtime_error("snapshot: bad wound row");
                    Wound wd;
                    wd.zone = static_cast<int>(Reader::parse_i64(g[0]));
                    wd.type = g[1];
                    wd.severity = static_cast<int>(Reader::parse_i64(g[2]));
                    wd.damage = static_cast<int>(Reader::parse_i64(g[3]));
                    wd.remaining = static_cast<int>(Reader::parse_i64(g[4]));
                    wd.bleed = static_cast<int>(Reader::parse_i64(g[5]));
                    wd.clot_hours = static_cast<int>(Reader::parse_i64(g[6]));
                    wd.treatment = static_cast<int>(Reader::parse_i64(g[7]));
                    wd.day = Reader::parse_i64(g[8]);
                    c.wounds.push_back(std::move(wd));
                }
                for (std::size_t j = 0; j < nl; ++j) {
                    std::vector<std::string> g = rd.next();
                    if (g.size() != 1) throw std::runtime_error("snapshot: bad lasting-effect row");
                    c.lasting.push_back(g[0]);
                }
                for (std::size_t j = 0; j < nd; ++j) {
                    std::vector<std::string> g = rd.next();
                    if (g.size() != 2) throw std::runtime_error("snapshot: bad durability row");
                    c.durability[g[0]] = static_cast<int>(Reader::parse_i64(g[1]));
                }
                const Id key = c.id;
                w.combat.by_actor[key] = std::move(c);
            }
            n = rd.section("COMBAT_HOSTILITY");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 3) throw std::runtime_error("snapshot: bad hostility row");
                w.combat.hostilities.push_back(Hostility{f[0], f[1], Reader::parse_i64(f[2])});
            }
            n = rd.section("COMBAT_DUELS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 3) throw std::runtime_error("snapshot: bad duel row");
                w.combat.duels.push_back(Duel{f[0], f[1], Reader::parse_i64(f[2])});
            }
            n = rd.section("COMBAT_PRISONERS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 3) throw std::runtime_error("snapshot: bad prisoner row");
                w.combat.prisoners.push_back(Prisoner{f[0], f[1], Reader::parse_i64(f[2])});
            }
            n = rd.section("COMBAT_DEATHS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 4) throw std::runtime_error("snapshot: bad death row");
                w.combat.deaths.push_back(DeathRecord{f[0], Reader::parse_i64(f[1]), f[2], f[3]});
            }
            w.combat.seq = Reader::parse_u64(rd.scalar("COMBAT_SEQ"));
        }

        // --- W5 DIVINE — optional: absent in a pre-W5 save, where the fresh
        // init() state (no wrath, no curse, nothing pending) stands.
        // Self-contained block — keep it delimited if you edit nearby.
        w.divine = DivineState{};
        if (rd.peek_tag() == "DIVINE_WRATH") {
            std::size_t n = rd.section("DIVINE_WRATH");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 3) throw std::runtime_error("snapshot: bad wrath row");
                w.divine.wrath_by_offender[f[0]][f[1]] = static_cast<int>(Reader::parse_i64(f[2]));
            }
            n = rd.section("DIVINE_CURSE");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad curse row");
                w.divine.curse_by_offender[f[0]] = f[1];
            }
            n = rd.section("DIVINE_PENDING");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 2) throw std::runtime_error("snapshot: bad pending-favour row");
                w.divine.pending_favour_penalty[f[0]] = static_cast<int>(Reader::parse_i64(f[1]));
            }
            w.divine.decay_acc = static_cast<int>(Reader::parse_i64(rd.scalar("DIVINE_DECAY_ACC")));
        }
        // --- end W5

        // --- P0a: goods in the world and in containers (absent before P0a).
        if (!rd.at_end() && rd.peek_tag() == "WORLD_ITEMS_NEXT") {
            w.world_items.next_id = Reader::parse_i64(rd.scalar("WORLD_ITEMS_NEXT"));
            std::size_t n = rd.section("WORLD_ITEMS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 15) throw std::runtime_error("snapshot: bad world item row");
                WorldItem wi;
                wi.id = f[0];
                wi.place = f[1];
                wi.x_cm = static_cast<int>(Reader::parse_i64(f[2]));
                wi.y_cm = static_cast<int>(Reader::parse_i64(f[3]));
                wi.z_cm = static_cast<int>(Reader::parse_i64(f[4]));
                wi.dropped_day = Reader::parse_i64(f[5]);
                wi.dropped_by = f[6];
                wi.stack = stack_from(f, 7, "world item");
                w.world_items.items.push_back(std::move(wi));
            }
            n = rd.section("CONTAINERS");
            for (std::size_t i = 0; i < n; ++i) {
                std::vector<std::string> f = rd.next();
                if (f.size() != 8) throw std::runtime_error("snapshot: bad container row");
                Container c;
                c.id = f[0];
                c.place = f[1];
                c.kind = f[2];
                c.owner = f[3];
                c.locked = Reader::parse_bool(f[4]);
                c.key_item = f[5];
                c.capacity_g = static_cast<int>(Reader::parse_i64(f[6]));
                const std::size_t stacks = static_cast<std::size_t>(Reader::parse_u64(f[7]));
                for (std::size_t j = 0; j < stacks; ++j) {
                    std::vector<std::string> sf = rd.next();
                    if (sf.size() != 8) throw std::runtime_error("snapshot: bad container stack row");
                    add_stack(c.contents, stack_from(sf, 0, "container"));
                }
                w.world_items.containers.emplace(c.id, std::move(c));
            }
        }
        // --- end P0a

        out = std::move(w);  // atomic: only reached once parsing fully succeeded
    } catch (const std::runtime_error&) {
        throw;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("snapshot: malformed save: ") + e.what());
    }
}

}  // namespace sim
