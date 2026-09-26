// Notices.cpp — FND-09: the player notice feed (sim/Notices.hpp).
#include "sim/Notices.hpp"

namespace sim {

void push_notice(NoticeState& s, DayNumber day, const std::string& key, const std::string& text) {
    s.recent.push_back(Notice{s.next_seq++, day, key, text});
    if (s.recent.size() > kNoticesKept) s.recent.erase(s.recent.begin());
}

const Notice* notice_at(const NoticeState& s, std::int64_t seq) {
    if (s.recent.empty() || seq < s.recent.front().seq || seq >= s.next_seq) return nullptr;
    return &s.recent[static_cast<std::size_t>(seq - s.recent.front().seq)];
}

}  // namespace sim
