// test_db.cpp — the canon loader reads the real database.
#include "sim/Db.hpp"

#include "sim/Test.hpp"
#include <fstream>
#include <filesystem>

using namespace sim;

static Db load_canon() { return Db::load("../db/canon"); }

static bool test_loads_every_table() {
    const Db db = load_canon();
    // Canon only grows (CANON rows are never deleted, D-018 appends): check
    // floors, not exact counts — exact counts went stale with every content
    // wave (W2-A appended a factions.csv row, temple_of_sun_and_moon, D-018;
    // see docs/proposals/invented-ledger-actions.md).
    SIM_CHECK(db.rows("deities").size() >= std::size_t{16});
    SIM_CHECK(db.rows("cities").size() >= std::size_t{9});
    SIM_CHECK(db.rows("factions").size() >= std::size_t{11});
    SIM_CHECK(db.rows("planetary_powers").size() >= std::size_t{6});
    return true;
}

static bool test_finds_rows_by_id() {
    const Db db = load_canon();
    const auto inanna = db.find("deities", "inanna");
    SIM_CHECK(inanna.has_value());
    SIM_CHECK_EQ(inanna->at("name"), std::string("Inanna"));
    SIM_CHECK(inanna->at("also_known_as").find("Ishtar") != std::string::npos);
    SIM_CHECK_EQ(db.find("deities", "no_such_god").has_value(), false);
    return true;
}

static bool test_quoted_fields_parse() {
    const Db db = load_canon();
    // planetary_powers:venus carries quoted, comma-laden canon text.
    const auto venus = db.find("planetary_powers", "venus");
    SIM_CHECK(venus.has_value());
    SIM_CHECK(venus->at("effects").find("sacred rites of Inanna") != std::string::npos);
    return true;
}

static bool test_open_rows_are_readable() {
    const Db db = load_canon();
    // Formerly OPEN, filled under D-018: the row stays readable with its new tag.
    const auto med = db.find("pantheons", "eight_deities_of_medicine");
    SIM_CHECK(med.has_value());
    SIM_CHECK(!med->at("tag").empty());
    return true;
}


// Bug-review 2026-09-25: CSV edge cases against the lint's own reading
// (tools/canon_lint.py: python csv + .strip() on id and tag).
static std::string write_fixture(const char* name, const std::string& events_csv) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "events.csv", std::ios::binary) << events_csv;
    return dir.string();
}

static bool test_quoted_newline_is_kept() {
    // RFC 4180: a line break inside a quoted field is part of the field.
    const Db db = Db::load(write_fixture("sim_db_fixture_newline",
        "id,summary,tag,source_ref\r\n"
        "e1,\"first line\r\nsecond line\",INVENTED,test\r\n"));
    const auto e1 = db.find("events", "e1");
    SIM_CHECK(e1.has_value());
    SIM_CHECK_EQ(e1->at("summary"), std::string("first line\nsecond line"));
    SIM_CHECK_EQ(e1->at("tag"), std::string("INVENTED"));
    return true;
}

static bool test_padded_id_and_tag_are_trimmed() {
    // canon_lint strips id and tag, so " OPEN" and " e2 " pass the lint; the
    // kernel must read them the same way or an OPEN row ships.
    const Db db = Db::load(write_fixture("sim_db_fixture_padded",
        "id,summary,tag,source_ref\n"
        " e2 ,x, OPEN ,test\n"));
    const auto e2 = db.find("events", "e2");
    SIM_CHECK(e2.has_value());
    SIM_CHECK_EQ(e2->at("tag"), std::string("OPEN"));
    return true;
}

SIM_MAIN(test_loads_every_table, test_finds_rows_by_id, test_quoted_fields_parse, test_open_rows_are_readable,
         test_quoted_newline_is_kept,
         test_padded_id_and_tag_are_trimmed)
