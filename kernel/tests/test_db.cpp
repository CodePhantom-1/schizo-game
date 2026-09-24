// test_db.cpp — the canon loader reads the real database.
#include "sim/Db.hpp"

#include "sim/Test.hpp"

using namespace sim;

static Db load_canon() { return Db::load("../db/canon"); }

static bool test_loads_every_table() {
    const Db db = load_canon();
    SIM_CHECK_EQ(db.rows("deities").size(), std::size_t{16});
    SIM_CHECK_EQ(db.rows("cities").size(), std::size_t{9});
    SIM_CHECK_EQ(db.rows("factions").size(), std::size_t{10});
    SIM_CHECK_EQ(db.rows("planetary_powers").size(), std::size_t{6});
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
    // The missing lists stay visible as OPEN rows until the designer fills them.
    const auto med = db.find("pantheons", "eight_deities_of_medicine");
    SIM_CHECK(med.has_value());
    SIM_CHECK_EQ(med->at("tag"), std::string("OPEN"));
    return true;
}

SIM_MAIN(test_loads_every_table, test_finds_rows_by_id, test_quoted_fields_parse, test_open_rows_are_readable)
