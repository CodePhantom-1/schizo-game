// Items.cpp — FND-01: item stacks (sim/Items.hpp).
#include "sim/Items.hpp"

#include <algorithm>
#include <sstream>
#include <tuple>

namespace sim {
namespace {

auto key(const ItemStack& s) {
    return std::tie(s.item, s.owner, s.stolen, s.bound, s.quality, s.made_day, s.condition);
}

int to_int(const std::string& s, int fallback) {
    if (s.empty() || s.size() > 9 || s.find_first_not_of("0123456789") != std::string::npos) return fallback;
    return std::stoi(s);
}

int saturate(long long n) { return static_cast<int>(std::min<long long>(n, kMaxStackQty)); }

}  // namespace

void normalize(Inventory& inv) {
    std::vector<ItemStack>& v = inv.stacks;
    std::stable_sort(v.begin(), v.end(),
                     [](const ItemStack& a, const ItemStack& b) { return key(a) < key(b); });
    std::vector<ItemStack> out;
    out.reserve(v.size());
    for (ItemStack& s : v) {
        if (s.qty <= 0) continue;
        if (!out.empty() && out.back().same_kind(s))
            out.back().qty = saturate(static_cast<long long>(out.back().qty) + s.qty);
        else
            out.push_back(std::move(s));
    }
    v = std::move(out);
}

int count_of(const Inventory& inv, const Id& item) {
    long long n = 0;
    for (const ItemStack& s : inv.stacks)
        if (s.item == item) n += s.qty;
    return saturate(n);
}

std::map<Id, int> counts(const Inventory& inv) {
    std::map<Id, int> out;
    for (const ItemStack& s : inv.stacks) out[s.item] = saturate(static_cast<long long>(out[s.item]) + s.qty);
    return out;
}

int add_stack(Inventory& inv, ItemStack s) {
    if (s.qty <= 0 || s.item.empty()) return count_of(inv, s.item);
    const Id item = s.item;
    inv.stacks.push_back(std::move(s));
    normalize(inv);
    return count_of(inv, item);
}

int add_items(Inventory& inv, const Id& item, int qty) {
    ItemStack s;
    s.item = item;
    s.qty = qty;
    return add_stack(inv, std::move(s));
}

std::vector<ItemStack> take_items(Inventory& inv, const Id& item, int qty) {
    std::vector<ItemStack> taken;
    if (qty <= 0) return taken;
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < inv.stacks.size(); ++i)
        if (inv.stacks[i].item == item) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const ItemStack& x = inv.stacks[a];
        const ItemStack& y = inv.stacks[b];
        return std::tie(x.made_day, x.quality) < std::tie(y.made_day, y.quality);
    });
    int left = qty;
    for (std::size_t i : order) {
        if (left == 0) break;
        ItemStack& s = inv.stacks[i];
        ItemStack part = s;
        part.qty = std::min(left, s.qty);
        s.qty -= part.qty;
        left -= part.qty;
        taken.push_back(std::move(part));
    }
    normalize(inv);
    return taken;
}

int set_count(Inventory& inv, const Id& item, int n) {
    const int have = count_of(inv, item);
    if (n > have) return add_items(inv, item, n - have);
    take_items(inv, item, have - std::max(n, 0));
    return count_of(inv, item);
}

std::optional<ItemStack> take_from_stack(Inventory& inv, std::size_t index, int qty) {
    if (index >= inv.stacks.size() || qty <= 0) return std::nullopt;
    ItemStack& s = inv.stacks[index];
    ItemStack part = s;
    part.qty = std::min(qty, s.qty);
    s.qty -= part.qty;
    normalize(inv);
    return part;
}

bool ItemDef::has_flag(const std::string& f) const {
    for (const std::string& x : flags)
        if (x.substr(0, x.find(':')) == f) return true;
    return false;
}

std::optional<ItemDef> item_def(const Db& db, const Id& item) {
    const std::optional<Row> row = db.find("items", item);
    if (!row || row->get("tag") == "OPEN") return std::nullopt;
    ItemDef d;
    d.id = item;
    d.ui_category = row->get("ui_category");
    d.weight_g = to_int(row->get("weight_g"), 0);
    d.stack_max = std::max(1, to_int(row->get("stack_max"), 1));
    d.spoil_days = to_int(row->get("spoil_days"), 0);
    std::stringstream ss(row->get("flags"));
    for (std::string f; std::getline(ss, f, '|');)
        if (!f.empty()) d.flags.push_back(f);
    return d;
}

long long stack_weight_g(const Db& db, const ItemStack& s) {
    const std::optional<ItemDef> d = item_def(db, s.item);
    return d ? static_cast<long long>(d->weight_g) * s.qty : 0;
}

}  // namespace sim
