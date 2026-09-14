#pragma once
#include "order_book.hpp"
#include <functional>
#include <vector>

namespace lob {
class MatchingEngine {
public:
    using TradeCallback = std::function<void(const Trade&)>;
    explicit MatchingEngine(TradeCallback callback = nullptr);
    [[nodiscard]] OrderStatus submit_order(OrderId id, Side side, Price price, Quantity quantity);
    [[nodiscard]] bool cancel_order(OrderId id);
    // new_quantity is the total quantity, including historical fills.
    [[nodiscard]] bool modify_order(OrderId id, Price new_price, Quantity new_quantity);
    [[nodiscard]] const OrderBook& get_order_book() const noexcept { return order_book_; }
    [[nodiscard]] std::vector<Trade> get_trades() noexcept {
        std::vector<Trade> result;
        trades_.swap(result);
        return result;
    }
private:
    OrderStatus match_order(Order* order);
    void notify_trades(std::size_t first);
    OrderBook order_book_;
    std::vector<Trade> trades_;
    TradeCallback trade_callback_;
};
}
