#pragma once
// Notices.hpp — FND-09: the player notice feed. The daily tick's outcomes that
// concern the player (a raid, a failed quest, a verdict against them) become
// notices the engine shows as toasts and keeps in the journal's log. Each
// notice has a sequence number that never repeats; the feed keeps only the
// newest kNoticesKept, so the engine reads from the last sequence it saw and
// an evicted one reads as nullptr, never as a wrong notice. Keys are
// string-table keys (notice.raid, notice.quest_failed, notice.verdict); text
// is the kernel's plain detail line.
#include "sim/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sim {

struct Notice {
    std::int64_t seq = 0;
    DayNumber day = 0;
    std::string key;
    std::string text;
};

struct NoticeState {
    std::vector<Notice> recent;   // oldest first, at most kNoticesKept
    std::int64_t next_seq = 0;    // the sequence the next notice gets
};

constexpr std::size_t kNoticesKept = 256;

void push_notice(NoticeState& s, DayNumber day, const std::string& key, const std::string& text);
// nullptr when seq was never written or has been evicted.
const Notice* notice_at(const NoticeState& s, std::int64_t seq);

}  // namespace sim
