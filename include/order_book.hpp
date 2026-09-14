#pragma once

#include "types.hpp"
#include "allocator/slab_allocator.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>

namespace lob {

class OrderBook {
public:
    OrderBook() = default;
    ~OrderBook();

    // Owns linked orders and slab storage; identity is stable.
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;

    [[nodiscard]] bool add_order(OrderId id, Side side,
                                  Price price, Quantity quantity);

    [[nodiscard]] bool cancel_order(OrderId id);
    [[nodiscard]] bool modify_order(OrderId id, Price new_price, Quantity new_quantity);
    [[nodiscard]] std::optional<Price> best_bid() const noexcept;
    [[nodiscard]] std::optional<Price> best_ask() const noexcept;
    [[nodiscard]] std::optional<Price> spread() const noexcept;
    [[nodiscard]] Quantity depth_at_price(Side side, Price price) const noexcept;
    [[nodiscard]] std::vector<std::pair<Price, Quantity>>
    get_levels(Side side, std::size_t n = 10) const;
    [[nodiscard]] const Order* get_order(OrderId id) const noexcept;
    [[nodiscard]] std::size_t order_count() const noexcept {
        return orders_.size();
    }
    void clear();
private:
    [[nodiscard]] Order* get_first_order_at_price(Side side, Price price) noexcept;
    void remove_order_from_level(Order* order);
    void remove_filled_order(Order* order);
    void update_price_level_quantity_incremental(Order* order, Quantity old_remaining);

    friend class MatchingEngine;

private:
    // Price level maintains a doubly-linked list of orders at the same price
    // Orders are added to the tail (FIFO) to maintain time priority
    struct PriceLevel {
        Price price;
        Quantity total_quantity{0};  // Sum of remaining quantities at this price
        Order* first_order{nullptr};  // Head of FIFO queue (oldest order)
        Order* last_order{nullptr};  // Tail of FIFO queue (newest order)

        // Add order to tail of linked list (maintains FIFO ordering)
        void add_order(Order* order) {
            order->next = nullptr;
            order->prev = last_order;
            if (last_order) {
                last_order->next = order;
            } else {
                first_order = order;  // First order in level
            }
            last_order = order;
            total_quantity += order->remaining();
        }

        // Remove order from linked list (O(1) operation)
        void remove_order(Order* order) {
            if (order->prev) {
                order->prev->next = order->next;
            } else {
                first_order = order->next;  // Was head
            }
            if (order->next) {
                order->next->prev = order->prev;
            } else {
                last_order = order->prev;  // Was tail
            }
            total_quantity -= order->remaining();
        }

        // Efficiently update total quantity when an order's remaining qty changes
        void update_quantity(Order* order, Quantity old_remaining) {
            total_quantity = total_quantity - old_remaining + order->remaining();
        }

        // Recalculate total quantity by walking the list (used for validation/debugging)
        // TODO: Consider removing if not needed, or add validation flag
        void update_order_quantity() {
            total_quantity = 0;
            Order* current = first_order;
            while (current) {
                total_quantity += current->remaining();
                current = current->next;
            }
        }

        [[nodiscard]] bool empty() const noexcept {
            return first_order == nullptr;
        }
    };

    // Bid levels: descending order (highest price first) using std::greater
    // Ask levels: ascending order (lowest price first) using default std::less
    using BidLevels = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskLevels = std::map<Price, PriceLevel>;

    void add_order_to_level(Order* order, PriceLevel& level);
    PriceLevel* get_price_level(Side side, Price price);
    const PriceLevel* get_price_level(Side side, Price price) const;

    Timestamp get_timestamp() const noexcept;

    BidLevels bid_levels_;
    AskLevels ask_levels_;
    std::unordered_map<OrderId, Order*> orders_;

    allocator::SlabAllocator<Order> allocator_;
};

} // namespace lob

