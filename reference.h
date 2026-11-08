// Deliberately naive reference model: one flat vector, linear scans, no maps.
// Independent of engine.h's data structures so a shared bug is unlikely.
#pragma once
#include "engine.h"

class RefModel {
 public:
  int process(const Order& in, std::vector<Trade>& out) {
    if (in.type == Type::Cancel) {
      for (size_t i = 0; i < book_.size(); ++i)
        if (book_[i].o.id == in.id) { book_.erase(book_.begin() + i); return 1; }
      return 0;
    }
    Order o = in;
    int n = 0;
    while (o.qty > 0) {
      int best = -1;  // best price, then lowest seq (earliest arrival)
      for (size_t i = 0; i < book_.size(); ++i) {
        const Order& m = book_[i].o;
        if (m.side == o.side) continue;
        if (o.type == Type::Limit &&
            !(o.side == Side::Buy ? m.price <= o.price : m.price >= o.price)) continue;
        if (best < 0) { best = (int)i; continue; }
        const Order& b = book_[best].o;
        bool better = (o.side == Side::Buy) ? m.price < b.price : m.price > b.price;
        if (better || (m.price == b.price && book_[i].seq < book_[best].seq)) best = (int)i;
      }
      if (best < 0) break;
      Order& m = book_[best].o;
      int32_t f = std::min(o.qty, m.qty);
      out.push_back({o.id, m.id, m.price, f});
      o.qty -= f; m.qty -= f; ++n;
      if (m.qty == 0) book_.erase(book_.begin() + best);
    }
    if (o.type == Type::Limit && o.qty > 0) book_.push_back({o, seq_++});
    return n;
  }
  size_t resting() const { return book_.size(); }

 private:
  struct Entry { Order o; uint64_t seq; };
  std::vector<Entry> book_;
  uint64_t seq_ = 0;
};
