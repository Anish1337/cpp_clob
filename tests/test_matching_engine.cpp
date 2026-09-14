#include <catch2/catch_test_macros.hpp>
#include "matching_engine.hpp"
#include <vector>

TEST_CASE("MatchingEngine - Limit order matching", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    // Add a sell order
    (void)engine.submit_order(1, lob::Side::Sell, 100, 10);
    
    // Add a buy order that matches
    auto status = engine.submit_order(2, lob::Side::Buy, 100, 5);
    
    REQUIRE(status == lob::OrderStatus::Filled);
    
    // Fully filled buy order should be removed from book
    const auto* buy_order = engine.get_order_book().get_order(2);
    REQUIRE(buy_order == nullptr);  // Order deallocated after full fill
    
    // Partially filled sell order should remain in book
    const auto* sell_order = engine.get_order_book().get_order(1);
    REQUIRE(sell_order != nullptr);
    REQUIRE(sell_order->filled_quantity == 5);
    REQUIRE(sell_order->remaining() == 5);
}

TEST_CASE("MatchingEngine - Partial fill", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    (void)engine.submit_order(1, lob::Side::Sell, 100, 5);
    auto status = engine.submit_order(2, lob::Side::Buy, 100, 10);
    
    REQUIRE(status == lob::OrderStatus::PartiallyFilled);
    
    const auto* buy_order = engine.get_order_book().get_order(2);
    REQUIRE(buy_order->filled_quantity == 5);
    REQUIRE(buy_order->remaining() == 5);
}

TEST_CASE("MatchingEngine - Price-time priority", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    // Add multiple sell orders at same price
    (void)engine.submit_order(1, lob::Side::Sell, 100, 5);
    (void)engine.submit_order(2, lob::Side::Sell, 100, 3);
    (void)engine.submit_order(3, lob::Side::Sell, 100, 2);
    
    // Buy order that matches all
    (void)engine.submit_order(4, lob::Side::Buy, 100, 10);
    
    // First order should be fully filled and removed
    const auto* order1 = engine.get_order_book().get_order(1);
    REQUIRE(order1 == nullptr);  // Fully filled orders are deallocated
    
    // Second order should be fully filled and removed
    const auto* order2 = engine.get_order_book().get_order(2);
    REQUIRE(order2 == nullptr);  // Fully filled orders are deallocated
    
    // The third order is also completely filled and removed
    const auto* order3 = engine.get_order_book().get_order(3);
    REQUIRE(order3 == nullptr);
    
    // Buy order should be fully filled and removed
    const auto* buy_order = engine.get_order_book().get_order(4);
    REQUIRE(buy_order == nullptr);  // Fully filled orders are deallocated
}

TEST_CASE("MatchingEngine - Trade generation", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    (void)engine.submit_order(1, lob::Side::Sell, 100, 10);
    (void)engine.submit_order(2, lob::Side::Buy, 100, 5);
    
    auto trades = engine.get_trades();
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].quantity == 5);
    REQUIRE(trades[0].price == 100);
    REQUIRE(trades[0].buy_order_id == 2);
    REQUIRE(trades[0].sell_order_id == 1);
}

TEST_CASE("MatchingEngine - Memory management for filled orders", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    // Add sell order
    (void)engine.submit_order(1, lob::Side::Sell, 100, 10);
    
    // Submit buy order that fully fills
    auto status = engine.submit_order(2, lob::Side::Buy, 100, 10);
    REQUIRE(status == lob::OrderStatus::Filled);
    
    // Both orders should be fully filled and removed from book
    const auto* buy_order = engine.get_order_book().get_order(2);
    REQUIRE(buy_order == nullptr);  // Fully filled order deallocated
    
    const auto* sell_order = engine.get_order_book().get_order(1);
    REQUIRE(sell_order == nullptr);  // Fully filled order deallocated
    
    // Book should be empty
    REQUIRE(engine.get_order_book().order_count() == 0);
}

TEST_CASE("MatchingEngine - Partially filled orders remain in book", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    // Add sell order
    (void)engine.submit_order(1, lob::Side::Sell, 100, 5);
    
    // Submit buy order that partially fills
    auto status = engine.submit_order(2, lob::Side::Buy, 100, 10);
    REQUIRE(status == lob::OrderStatus::PartiallyFilled);
    
    // Buy order should remain in book (partially filled)
    const auto* buy_order = engine.get_order_book().get_order(2);
    REQUIRE(buy_order != nullptr);
    REQUIRE(buy_order->filled_quantity == 5);
    REQUIRE(buy_order->remaining() == 5);
    
    // Sell order should be removed (fully filled)
    const auto* sell_order = engine.get_order_book().get_order(1);
    REQUIRE(sell_order == nullptr);  // Fully filled order deallocated
    
    // Book should have 1 order remaining
    REQUIRE(engine.get_order_book().order_count() == 1);
}

TEST_CASE("Matching preserves depth, status and trade records on both sides", "[matching_engine]") {
    for (const auto side : {lob::Side::Buy, lob::Side::Sell}) {
        lob::MatchingEngine engine;
        const auto opposite = side == lob::Side::Buy ? lob::Side::Sell : lob::Side::Buy;
        REQUIRE(engine.submit_order(1, opposite, 100, 5) == lob::OrderStatus::New);
        REQUIRE(engine.submit_order(2, side, 100, 10) == lob::OrderStatus::PartiallyFilled);
        const auto& book = engine.get_order_book();
        REQUIRE(book.depth_at_price(side, 100) == 5);
        REQUIRE(book.depth_at_price(opposite, 100) == 0);
        REQUIRE(engine.get_trades().size() == 1);
        REQUIRE(engine.submit_order(3, opposite, 100, 2) == lob::OrderStatus::Filled);
        REQUIRE(book.get_order(2)->status == lob::OrderStatus::PartiallyFilled);
        REQUIRE(book.depth_at_price(side, 100) == 3);
        REQUIRE(engine.get_trades().size() == 1);
        REQUIRE(engine.cancel_order(2));
        REQUIRE(book.order_count() == 0);
        REQUIRE_FALSE(book.best_bid());
        REQUIRE_FALSE(book.best_ask());
    }
}

TEST_CASE("Replacement preserves total quantity and executes crossed prices", "[matching_engine]") {
    lob::MatchingEngine engine;
    REQUIRE(engine.submit_order(1, lob::Side::Sell, 100, 10) == lob::OrderStatus::New);
    REQUIRE(engine.submit_order(2, lob::Side::Buy, 100, 6) == lob::OrderStatus::Filled);
    REQUIRE(engine.modify_order(1, 101, 10));
    const auto& book = engine.get_order_book();
    REQUIRE(book.get_order(1)->quantity == 10);
    REQUIRE(book.get_order(1)->filled_quantity == 6);
    REQUIRE(book.get_order(1)->remaining() == 4);
    REQUIRE(book.depth_at_price(lob::Side::Sell, 101) == 4);
    REQUIRE_FALSE(engine.modify_order(1, 99, 5));
    REQUIRE(book.get_order(1)->price == 101);
    REQUIRE(engine.submit_order(3, lob::Side::Buy, 99, 4) == lob::OrderStatus::New);
    REQUIRE(engine.modify_order(1, 99, 10));
    REQUIRE(book.order_count() == 0);
    const auto trades = engine.get_trades();
    REQUIRE(trades.size() == 2);
    REQUIRE(trades.back().price == 99);
    REQUIRE(trades.back().quantity == 4);
}

TEST_CASE("Reductions retain FIFO, increases and repricing lose FIFO", "[matching_engine]") {
    for (int modification = 0; modification < 3; ++modification) {
        lob::MatchingEngine engine;
        (void)engine.submit_order(1, lob::Side::Sell, modification == 2 ? 101 : 100, 10);
        (void)engine.submit_order(2, lob::Side::Sell, 100, 10);
        REQUIRE(engine.modify_order(1, 100, modification == 0 ? 5 : 15));
        REQUIRE(engine.submit_order(3, lob::Side::Buy, 100, 1) == lob::OrderStatus::Filled);
        const auto trades = engine.get_trades();
        REQUIRE(trades.size() == 1);
        REQUIRE(trades[0].sell_order_id == (modification == 0 ? 1 : 2));
    }
}

TEST_CASE("Price priority, maker pricing, and residual depth", "[matching_engine]") {
    lob::MatchingEngine engine;
    (void)engine.submit_order(1, lob::Side::Sell, 102, 4);
    (void)engine.submit_order(2, lob::Side::Sell, 100, 3);
    (void)engine.submit_order(3, lob::Side::Sell, 101, 2);
    REQUIRE(engine.submit_order(4, lob::Side::Buy, 101, 7) == lob::OrderStatus::PartiallyFilled);
    auto trades = engine.get_trades();
    REQUIRE(trades.size() == 2);
    REQUIRE(trades[0].sell_order_id == 2);
    REQUIRE(trades[0].price == 100);
    REQUIRE(trades[1].sell_order_id == 3);
    REQUIRE(trades[1].price == 101);
    REQUIRE(engine.get_order_book().depth_at_price(lob::Side::Buy, 101) == 2);
    REQUIRE(engine.get_order_book().depth_at_price(lob::Side::Sell, 102) == 4);
}

TEST_CASE("Invalid requests leave orders unchanged and total equal to fills cancels", "[matching_engine]") {
    lob::MatchingEngine engine;
    REQUIRE(engine.submit_order(1, lob::Side::Buy, 100, 0) == lob::OrderStatus::Rejected);
    REQUIRE(engine.submit_order(1, lob::Side::Buy, 100, 10) == lob::OrderStatus::New);
    REQUIRE(engine.submit_order(1, lob::Side::Sell, 99, 1) == lob::OrderStatus::Rejected);
    REQUIRE(engine.submit_order(2, lob::Side::Sell, 100, 6) == lob::OrderStatus::Filled);
    REQUIRE(engine.modify_order(1, 100, 6));
    REQUIRE(engine.get_order_book().order_count() == 0);
    REQUIRE_FALSE(engine.cancel_order(1));
    REQUIRE_FALSE(engine.modify_order(1, 100, 10));
}

TEST_CASE("Callbacks see a consistent book and can drain trade records", "[matching_engine]") {
    lob::MatchingEngine* pointer = nullptr;
    std::vector<lob::Trade> observed;
    lob::MatchingEngine engine([&](const lob::Trade& trade) {
        REQUIRE(pointer->get_order_book().order_count() == 0);
        observed.push_back(trade);
        (void)pointer->get_trades();
    });
    pointer = &engine;
    (void)engine.submit_order(1, lob::Side::Sell, 100, 2);
    (void)engine.submit_order(2, lob::Side::Sell, 100, 3);
    REQUIRE(engine.submit_order(3, lob::Side::Buy, 100, 5) == lob::OrderStatus::Filled);
    REQUIRE(observed.size() == 2);
}

#include <map>
#include <random>
TEST_CASE("Seeded requests agree with a simple reference matcher", "[matching_engine]") {
    struct Entry { lob::Side side; lob::Price price; lob::Quantity total, filled; std::size_t priority; };
    std::map<lob::OrderId, Entry> model;
    std::mt19937 random(123);
    lob::MatchingEngine engine;
    std::size_t sequence = 0;
    lob::OrderId next_id = 1;
    for (int step = 0; step < 1500; ++step) {
        INFO("step " << step);
        lob::OrderId incoming = 0;
        if (model.empty() || random() % 3 == 0) {
            incoming = next_id++;
            Entry entry{random()%2 ? lob::Side::Buy : lob::Side::Sell,
                        95 + static_cast<lob::Price>(random()%11), 1 + random()%20, 0, sequence++};
            (void)engine.submit_order(incoming, entry.side, entry.price, entry.total);
            model.emplace(incoming, entry);
        } else {
            auto it = model.begin();
            std::advance(it, random() % model.size());
            if (random() % 2) {
                REQUIRE(engine.cancel_order(it->first));
                model.erase(it);
            } else {
                incoming = it->first;
                auto& entry = it->second;
                const auto price = 95 + static_cast<lob::Price>(random()%11);
                const auto total = entry.filled + random()%21;
                REQUIRE(engine.modify_order(incoming, price, total));
                if (total == entry.filled) {
                    model.erase(it);
                    incoming = 0;
                } else {
                    if (price != entry.price || total > entry.total) entry.priority = sequence++;
                    entry.price = price; entry.total = total;
                }
            }
        }
        std::vector<lob::Trade> expected;
        if (incoming) {
            auto& taker = model.at(incoming);
            while (taker.filled < taker.total) {
                auto best = model.end();
                for (auto it = model.begin(); it != model.end(); ++it) {
                    const auto& maker = it->second;
                    if (maker.side == taker.side) continue;
                    if (taker.side == lob::Side::Buy ? maker.price > taker.price : maker.price < taker.price) continue;
                    if (best == model.end() ||
                        (taker.side == lob::Side::Buy ? maker.price < best->second.price : maker.price > best->second.price) ||
                        (maker.price == best->second.price && maker.priority < best->second.priority)) best = it;
                }
                if (best == model.end()) break;
                auto& maker = best->second;
                const auto amount = std::min(taker.total - taker.filled, maker.total - maker.filled);
                expected.push_back({taker.side == lob::Side::Buy ? incoming : best->first,
                                    taker.side == lob::Side::Sell ? incoming : best->first,
                                    maker.price, amount, {}});
                taker.filled += amount; maker.filled += amount;
                if (maker.filled == maker.total) model.erase(best);
            }
            if (taker.filled == taker.total) model.erase(incoming);
        }
        const auto actual = engine.get_trades();
        REQUIRE(actual.size() == expected.size());
        for (std::size_t i = 0; i < actual.size(); ++i) {
            REQUIRE(actual[i].buy_order_id == expected[i].buy_order_id);
            REQUIRE(actual[i].sell_order_id == expected[i].sell_order_id);
            REQUIRE(actual[i].price == expected[i].price);
            REQUIRE(actual[i].quantity == expected[i].quantity);
        }
        const auto& book = engine.get_order_book();
        REQUIRE(book.order_count() == model.size());
        std::map<std::pair<lob::Side, lob::Price>, lob::Quantity> depths;
        for (const auto& [id, entry] : model) {
            const auto* order = book.get_order(id);
            REQUIRE(order != nullptr);
            REQUIRE(order->quantity == entry.total);
            REQUIRE(order->filled_quantity == entry.filled);
            REQUIRE(order->price == entry.price);
            REQUIRE(order->status == (entry.filled ? lob::OrderStatus::PartiallyFilled : lob::OrderStatus::New));
            depths[{entry.side, entry.price}] += entry.total - entry.filled;
        }
        for (auto side : {lob::Side::Buy, lob::Side::Sell}) {
            std::size_t count = 0;
            for (const auto& [key, value] : depths) if (key.first == side) {
                REQUIRE(book.depth_at_price(side, key.second) == value);
                ++count;
            }
            REQUIRE(book.get_levels(side, 100).size() == count);
        }
        if (book.best_bid() && book.best_ask()) REQUIRE(*book.best_bid() < *book.best_ask());
    }
}
