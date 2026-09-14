#include "matching_engine.hpp"
#include <algorithm>
#include <utility>

namespace lob {
MatchingEngine::MatchingEngine(TradeCallback callback)
    : trade_callback_(std::move(callback)) {}

OrderStatus MatchingEngine::submit_order(OrderId id, Side side, Price price, Quantity quantity) {
    if (!order_book_.add_order(id, side, price, quantity)) return OrderStatus::Rejected;
    const auto first = trades_.size();
    const auto status = match_order(order_book_.orders_.at(id));
    notify_trades(first);
    return status;
}

bool MatchingEngine::cancel_order(OrderId id) { return order_book_.cancel_order(id); }

bool MatchingEngine::modify_order(OrderId id, Price price, Quantity quantity) {
    if (!order_book_.modify_order(id, price, quantity)) return false;
    auto it = order_book_.orders_.find(id);
    if (it == order_book_.orders_.end()) return true;
    const auto first = trades_.size();
    (void)match_order(it->second);
    notify_trades(first);
    return true;
}

OrderStatus MatchingEngine::match_order(Order* order) {
    const Side opposite = order->side == Side::Buy ? Side::Sell : Side::Buy;
    while (!order->is_filled()) {
        auto best = opposite == Side::Sell ? order_book_.best_ask() : order_book_.best_bid();
        if (!best || (order->side == Side::Buy ? order->price < *best : order->price > *best)) break;
        Order* resting = order_book_.get_first_order_at_price(opposite, *best);
        const Quantity incoming_remaining = order->remaining();
        const Quantity resting_remaining = resting->remaining();
        const Quantity quantity = std::min(incoming_remaining, resting_remaining);
        // Reserve the trade record before changing quantities.
        trades_.push_back(Trade{
            .buy_order_id = order->side == Side::Buy ? order->id : resting->id,
            .sell_order_id = order->side == Side::Sell ? order->id : resting->id,
            .price = *best,
            .quantity = quantity,
            .timestamp = std::chrono::duration_cast<Timestamp>(std::chrono::steady_clock::now().time_since_epoch())
        });
        order->filled_quantity += quantity;
        resting->filled_quantity += quantity;
        order_book_.update_price_level_quantity_incremental(order, incoming_remaining);
        order_book_.update_price_level_quantity_incremental(resting, resting_remaining);
        resting->status = resting->is_filled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
        if (resting->is_filled()) order_book_.remove_filled_order(resting);
    }
    order->status = order->is_filled() ? OrderStatus::Filled :
        order->filled_quantity ? OrderStatus::PartiallyFilled : OrderStatus::New;
    const auto status = order->status;
    if (order->is_filled()) order_book_.remove_filled_order(order);
    return status;
}

void MatchingEngine::notify_trades(std::size_t first) {
    if (!trade_callback_) return;
    // Notify only after the book is consistent. Callbacks may inspect the book
    // or drain recorded trades without invalidating the notification batch.
    const std::vector<Trade> notifications(trades_.begin() + first, trades_.end());
    for (const auto& trade : notifications) trade_callback_(trade);
}
}
