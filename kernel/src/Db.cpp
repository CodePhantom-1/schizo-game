// Db.cpp — RFC-4180-ish CSV parsing: quoted fields, doubled quotes, CRLF safe.
#include "sim/Db.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace sim {
namespace {

// Python's str.strip() whitespace set, as canon_lint.py applies it.
std::string strip(const std::string& s) {
    const char* ws = " \t\r\n\f\v";
    const std::size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::vector<std::vector<std::string>> parse_csv(std::istream& in) {
    std::vector<std::vector<std::string>> out;
    std::string line;
    std::vector<std::string> row;
    bool in_quotes = false;
    bool row_started = false;

    auto end_field = [&] {
        row_started = true;
        row.push_back("");
    };
    auto end_row = [&] {
        if (row_started || !row.empty()) {
            out.push_back(row);
            row.clear();
            row_started = false;
        }
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::size_t i = 0;
        while (i < line.size()) {
            char c = line[i];
            if (in_quotes) {
                if (c == '"') {
                    if (i + 1 < line.size() && line[i + 1] == '"') {
                        row.back() += '"';
                        i += 2;
                        continue;
                    }
                    in_quotes = false;
                    ++i;
                    continue;
                }
                row.back() += c;
                ++i;
                continue;
            }
            if (c == '"') {
                if (!row_started) end_field();
                in_quotes = true;
                ++i;
                continue;
            }
            if (c == ',') {
                if (!row_started) end_field();
                row_started = false;  // comma ends the field
                ++i;
                continue;
            }
            if (!row_started) end_field();
            row.back() += c;
            ++i;
        }
        if (in_quotes) {  // quoted newline: part of the field (RFC 4180); keep reading
            row.back() += '\n';
            continue;
        }
        if (!row_started) end_field();
        end_row();
    }
    end_row();
    return out;
}

}  // namespace

Db Db::load(const std::string& canon_dir) {
    Db db;
    const std::vector<std::string> tables = {
        "buildings", "callings", "calendar", "cities", "customs", "deities",
        "dialogues", "divination_forms", "endings", "events", "factions",
        "foods", "items", "laws", "magick_forms", "names", "pantheons",
        "people", "places", "planetary_powers", "quests", "ranks", "regions", "rites",
        "rite_teachings",  // K-1: who/what teaches each rite
        "schedules", "seasons", "skills", "story", "talents",
        "terrain_schemes", "treaties", "world_lore", "recipes",
        "festivals", "person_schedules",  // K-2
        "attributes", "skill_teachings", "work_roles",  // W4-A
        "arms", "combat_styles"};         // W4-B: combat

    for (const std::string& table : tables) {
        std::ifstream in(canon_dir + "/" + table + ".csv");
        if (!in) continue;  // a table may not exist yet
        auto parsed = parse_csv(in);
        if (parsed.size() < 1) continue;
        const std::vector<std::string>& header = parsed[0];
        int id_col = -1;
        for (std::size_t c = 0; c < header.size(); ++c)
            if (header[c] == "id") id_col = static_cast<int>(c);
        if (id_col < 0)
            throw std::runtime_error("canon table without id column: " + table);
        for (std::size_t r = 1; r < parsed.size(); ++r) {
            Row row;
            for (std::size_t c = 0; c < header.size() && c < parsed[r].size(); ++c)
                row.fields[header[c]] = parsed[r][c];
            // canon_lint.py reads id and tag with .strip(): " OPEN " passes the
            // lint as OPEN, so the kernel must see it as OPEN too.
            for (const char* col : {"id", "tag"}) {
                auto f = row.fields.find(col);
                if (f != row.fields.end()) f->second = strip(f->second);
            }
            if ((row.fields["id"]).empty()) continue;
            db.tables_[table].push_back(std::move(row));
        }
    }
    return db;
}

const std::vector<Row>& Db::rows(const std::string& table) const {
    static const std::vector<Row> empty;
    auto it = tables_.find(table);
    return it == tables_.end() ? empty : it->second;
}

std::optional<Row> Db::find(const std::string& table, const std::string& id) const {
    for (const Row& r : rows(table))
        if (r.at("id") == id) return r;
    return std::nullopt;
}

bool Db::has(const std::string& table, const std::string& id) const {
    return find(table, id).has_value();
}

}  // namespace sim
