#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <compare>

namespace lob {

using Price = std::int64_t;  // Price in ticks (e.g., cents for USD)
using Quantity = std::uint64_t;
using OrderId = std::uint64_t;
using Timestamp = std::chrono::nanoseconds;

enum class Side : std::uint8_t {
    Buy = 0,
    Sell = 1
};


enum class OrderStatus : std::uint8_t {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    Rejected = 4
};

struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
    Quantity filled_quantity{0};
    Timestamp timestamp;
    OrderStatus status{OrderStatus::New};
    
    Order* next{nullptr};
    Order* prev{nullptr};
    
    auto operator<=>(const Order& other) const noexcept {
        if (side != other.side) {
            return side <=> other.side;
        }
        if (side == Side::Buy) {
            // Buy orders: higher price first, then earlier time
            if (price != other.price) {
                return other.price <=> price;  // Descending price
            }
        } else {
            // Sell orders: lower price first, then earlier time
            if (price != other.price) {
                return price <=> other.price;  // Ascending price
            }
        }
        return timestamp <=> other.timestamp;  // Earlier time first
    }
    
    [[nodiscard]] Quantity remaining() const noexcept {
        return quantity - filled_quantity;
    }
    
    [[nodiscard]] bool is_filled() const noexcept {
        return filled_quantity >= quantity;
    }
};

struct Trade {
    OrderId buy_order_id;
    OrderId sell_order_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};

} // namespace lob

