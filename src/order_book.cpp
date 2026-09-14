#include "order_book.hpp"
#include <chrono>
#include <limits>
#include <ranges>  // C++23: std::ranges::to

namespace lob {

OrderBook::~OrderBook() {
    clear();
}

bool OrderBook::add_order(OrderId id, Side side,
                          Price price, Quantity quantity) {
    if (quantity == 0 || (side != Side::Buy && side != Side::Sell)) {
        return false;
    }

    if (orders_.find(id) != orders_.end()) {
        return false;  // Order ID already exists
    }

    const auto* existing_level = get_price_level(side, price);
    if (existing_level && quantity > std::numeric_limits<Quantity>::max() - existing_level->total_quantity)
        return false;

    // Reuse pooled order storage; growing the pool can allocate.
    Order* order = allocator_.allocate();
    if (!order) {
        return false;
    }

    // Initialize order fields
    order->id = id;
    order->side = side;
    order->price = price;
    order->quantity = quantity;
    order->filled_quantity = 0;
    order->timestamp = get_timestamp();  // Used for FIFO ordering within price level
    order->status = OrderStatus::New;
    order->next = nullptr;
    order->prev = nullptr;

    PriceLevel* level = nullptr;
    try {
        if (side == Side::Buy) level = &bid_levels_.try_emplace(price).first->second;
        else level = &ask_levels_.try_emplace(price).first->second;
        level->price = price;
        orders_.emplace(id, order);
    } catch (...) {
        if (level && level->empty()) {
            if (side == Side::Buy) bid_levels_.erase(price);
            else ask_levels_.erase(price);
        }
        allocator_.deallocate(order);
        throw;
    }
    add_order_to_level(order, *level);

    return true;
}

bool OrderBook::cancel_order(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }

    Order* order = it->second;
    if (order->is_filled()) {
        return false;
    }

    remove_order_from_level(order);
    orders_.erase(it);
    allocator_.deallocate(order);

    return true;
}

bool OrderBook::modify_order(OrderId id, Price new_price, Quantity new_quantity) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return false;
    Order* order = it->second;
    if (new_quantity < order->filled_quantity) return false;
    if (new_quantity == order->filled_quantity) return cancel_order(id);

    const Quantity remaining = new_quantity - order->filled_quantity;
    const Quantity old_remaining = order->remaining();
    auto* target = get_price_level(order->side, new_price);
    const Quantity existing = target ? target->total_quantity : 0;
    const Quantity other = existing - (new_price == order->price ? old_remaining : 0);
    if (remaining > std::numeric_limits<Quantity>::max() - other) return false;

    // Same-price reductions retain priority. Increases and repricing lose it.
    if (new_price == order->price && new_quantity <= order->quantity) {
        order->quantity = new_quantity;
        target->update_quantity(order, old_remaining);
        return true;
    }
    // Allocate the destination level before unlinking the order.
    if (!target) {
        if (order->side == Side::Buy) target = &bid_levels_.try_emplace(new_price).first->second;
        else target = &ask_levels_.try_emplace(new_price).first->second;
        target->price = new_price;
    }
    if (new_price == order->price) {
        // Keep the level alive while moving its order to the tail.
        target->remove_order(order);
    } else {
        remove_order_from_level(order);
    }
    order->price = new_price;
    order->quantity = new_quantity;
    order->timestamp = get_timestamp();
    target->add_order(order);
    return true;
}

std::optional<Price> OrderBook::best_bid() const noexcept {
    // Best bid is highest buy price (begin() of descending map)
    if (bid_levels_.empty()) {
        return std::nullopt;
    }
    return bid_levels_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const noexcept {
    // Best ask is lowest sell price (begin() of ascending map)
    if (ask_levels_.empty()) {
        return std::nullopt;
    }
    return ask_levels_.begin()->first;
}

std::optional<Price> OrderBook::spread() const noexcept {
    const auto bid = best_bid(), ask = best_ask();
    if (!bid || !ask) return std::nullopt;
    // Signed tick prices are supported; their difference may not fit Price.
    if ((*bid < 0 && *ask > std::numeric_limits<Price>::max() + *bid) ||
        (*bid > 0 && *ask < std::numeric_limits<Price>::min() + *bid)) return std::nullopt;
    return *ask - *bid;
}

Quantity OrderBook::depth_at_price(Side side, Price price) const noexcept {
    const PriceLevel* level = get_price_level(side, price);
    if (!level) {
        return 0;
    }
    return level->total_quantity;
}

std::vector<std::pair<Price, Quantity>> OrderBook::get_levels(Side side, std::size_t n) const {
    // Returns top N price levels (market depth)
    // C++23: Use std::ranges::to for cleaner range-to-container conversion
    namespace r = std::ranges;

    if (side == Side::Buy) {
        // Bids: iterate from highest to lowest price
        return bid_levels_
            | r::views::take(n)
            | r::views::transform([](const auto& pair) {
                return std::make_pair(pair.first, pair.second.total_quantity);
            })
            | r::to<std::vector>();
    } else {
        // Asks: iterate from lowest to highest price
        return ask_levels_
            | r::views::take(n)
            | r::views::transform([](const auto& pair) {
                return std::make_pair(pair.first, pair.second.total_quantity);
            })
            | r::to<std::vector>();
    }
}

const Order* OrderBook::get_order(OrderId id) const noexcept {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return nullptr;
    }
    return it->second;
}

void OrderBook::clear() {
    for (auto& [id, order] : orders_) {
        allocator_.deallocate(order);
    }
    orders_.clear();
    bid_levels_.clear();
    ask_levels_.clear();
}

Order* OrderBook::get_first_order_at_price(Side side, Price price) noexcept {
    PriceLevel* level = get_price_level(side, price);
    if (!level || level->empty()) {
        return nullptr;
    }
    return level->first_order;
}

void OrderBook::remove_order_from_level(Order* order) {
    if (!order) {
        return;
    }

    PriceLevel* level = get_price_level(order->side, order->price);
    if (level) {
        level->remove_order(order);

        // Remove empty price level
        if (level->empty()) {
            if (order->side == Side::Buy) {
                bid_levels_.erase(order->price);
            } else {
                ask_levels_.erase(order->price);
            }
        }
    }
}

void OrderBook::remove_filled_order(Order* order) {
    if (!order || !order->is_filled()) {
        return;
    }

    remove_order_from_level(order);
    auto it = orders_.find(order->id);
    if (it != orders_.end()) {
        orders_.erase(it);
        allocator_.deallocate(order);
    }
}

void OrderBook::update_price_level_quantity_incremental(Order* order, Quantity old_remaining) {
    if (!order) {
        return;
    }

    PriceLevel* level = get_price_level(order->side, order->price);
    if (level) {
        level->update_quantity(order, old_remaining);
    }
}

void OrderBook::add_order_to_level(Order* order, PriceLevel& level) {
    level.add_order(order);
}

OrderBook::PriceLevel* OrderBook::get_price_level(Side side, Price price) {
    // O(log n) lookup in price level map
    // TODO: Consider templating this to reduce duplication between const/non-const versions
    if (side == Side::Buy) {
        auto it = bid_levels_.find(price);
        return (it != bid_levels_.end()) ? &it->second : nullptr;
    } else {
        auto it = ask_levels_.find(price);
        return (it != ask_levels_.end()) ? &it->second : nullptr;
    }
}

const OrderBook::PriceLevel* OrderBook::get_price_level(Side side, Price price) const {
    // Const version for read-only access
    if (side == Side::Buy) {
        auto it = bid_levels_.find(price);
        return (it != bid_levels_.end()) ? &it->second : nullptr;
    } else {
        auto it = ask_levels_.find(price);
        return (it != ask_levels_.end()) ? &it->second : nullptr;
    }
}

Timestamp OrderBook::get_timestamp() const noexcept {
    return std::chrono::duration_cast<Timestamp>(
        std::chrono::steady_clock::now().time_since_epoch()
    );
}

} // namespace lob
