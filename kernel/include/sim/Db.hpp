#pragma once
// Db.hpp — read-only access to db/canon (contract, coordinator-owned).
// Modules read canon data ONLY through this; nobody else parses CSVs.
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace sim {

struct Row {
    std::map<std::string, std::string> fields;
    const std::string& at(const std::string& col) const { return fields.at(col); }
    std::string get(const std::string& col, const std::string& fallback = "") const {
        auto it = fields.find(col);
        return it == fields.end() ? fallback : it->second;
    }
};

class Db {
public:
    // Loads every *.csv in the directory. Throws std::runtime_error on a
    // malformed file or a missing id column.
    static Db load(const std::string& canon_dir);

    const std::vector<Row>& rows(const std::string& table) const;
    std::optional<Row> find(const std::string& table, const std::string& id) const;
    bool has(const std::string& table, const std::string& id) const;

private:
    std::map<std::string, std::vector<Row>> tables_;
};

}  // namespace sim
