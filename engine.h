// Price-time priority matching engine. Single-threaded core; feed it via SPSC queue.
#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

enum class Side : uint8_t { Buy, Sell };
enum class Type : uint8_t { Limit, Market, Cancel };

struct Order {
  uint64_t id;     // unique per new order; for Cancel, the id to cancel
  int64_t price;   // ignored for Market/Cancel
  int32_t qty;     // ignored for Cancel
  Side side;
  Type type;
};

struct Trade {
  uint64_t taker, maker;
  int64_t price;  // always the resting (maker) price
  int32_t qty;
  bool operator==(const Trade& o) const {
    return taker == o.taker && maker == o.maker && price == o.price && qty == o.qty;
  }
};

class Engine {
 public:
  // Appends fills to `out`. Returns #trades for Limit/Market, and 1/0 (cancelled / not found) for Cancel.
  // Limit remainder rests; Market remainder is dropped. Caller guarantees new-order ids are unique.
  int process(const Order& in, std::vector<Trade>& out) {
    if (in.type == Type::Cancel) return cancel(in.id) ? 1 : 0;
    Order o = in;
    int n = (o.side == Side::Buy)
                ? match(asks_, o, [](int64_t ask, int64_t lim) { return ask <= lim; }, out)
                : match(bids_, o, [](int64_t bid, int64_t lim) { return bid >= lim; }, out);
    if (o.type == Type::Limit && o.qty > 0) {
      auto& lvl = (o.side == Side::Buy) ? bids_[o.price] : asks_[o.price];
      lvl.push_back(o);
      index_[o.id] = {o.side, o.price, std::prev(lvl.end())};
    }
    return n;
  }

  size_t resting() const { return index_.size(); }

 private:
  using Level = std::list<Order>;
  struct Loc { Side side; int64_t price; Level::iterator it; };

  std::map<int64_t, Level, std::greater<int64_t>> bids_;  // best = begin()
  std::map<int64_t, Level> asks_;                         // best = begin()
  std::unordered_map<uint64_t, Loc> index_;

  template <class Book, class Crosses>
  int match(Book& book, Order& o, Crosses crosses, std::vector<Trade>& out) {
    int n = 0;
    while (o.qty > 0 && !book.empty()) {
      auto lvl = book.begin();
      if (o.type == Type::Limit && !crosses(lvl->first, o.price)) break;
      Level& q = lvl->second;
      while (o.qty > 0 && !q.empty()) {
        Order& m = q.front();
        int32_t f = std::min(o.qty, m.qty);
        out.push_back({o.id, m.id, lvl->first, f});
        o.qty -= f; m.qty -= f; ++n;
        if (m.qty == 0) { index_.erase(m.id); q.pop_front(); }
      }
      if (q.empty()) book.erase(lvl);
    }
    return n;
  }

  bool cancel(uint64_t id) {
    auto it = index_.find(id);
    if (it == index_.end()) return false;
    Loc loc = it->second;
    index_.erase(it);
    if (loc.side == Side::Buy) {
      auto lv = bids_.find(loc.price);
      lv->second.erase(loc.it);
      if (lv->second.empty()) bids_.erase(lv);
    } else {
      auto lv = asks_.find(loc.price);
      lv->second.erase(loc.it);
      if (lv->second.empty()) asks_.erase(lv);
    }
    return true;
  }
};
