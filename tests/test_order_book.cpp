#include <catch2/catch_test_macros.hpp>
#include "order_book.hpp"
#include <vector>

TEST_CASE("OrderBook - Add and retrieve orders", "[order_book]") {
    lob::OrderBook book;
    
    REQUIRE(book.add_order(1, lob::Side::Buy, 100, 10));
    REQUIRE(book.add_order(2, lob::Side::Sell, 101, 5));
    
    REQUIRE(book.order_count() == 2);
    
    const auto* order1 = book.get_order(1);
    REQUIRE(order1 != nullptr);
    REQUIRE(order1->side == lob::Side::Buy);
    REQUIRE(order1->price == 100);
    REQUIRE(order1->quantity == 10);
}

TEST_CASE("OrderBook - Best bid/ask", "[order_book]") {
    lob::OrderBook book;
    
    (void)book.add_order(1, lob::Side::Buy, 100, 10);
    (void)book.add_order(2, lob::Side::Buy, 99, 5);
    (void)book.add_order(3, lob::Side::Sell, 101, 10);
    (void)book.add_order(4, lob::Side::Sell, 102, 5);
    
    auto best_bid = book.best_bid();
    REQUIRE(best_bid.has_value());
    REQUIRE(*best_bid == 100);  // Highest buy price
    
    auto best_ask = book.best_ask();
    REQUIRE(best_ask.has_value());
    REQUIRE(*best_ask == 101);  // Lowest sell price
}

TEST_CASE("OrderBook - Spread calculation", "[order_book]") {
    lob::OrderBook book;
    
    (void)book.add_order(1, lob::Side::Buy, 100, 10);
    (void)book.add_order(2, lob::Side::Sell, 101, 10);
    
    auto spread = book.spread();
    REQUIRE(spread.has_value());
    REQUIRE(*spread == 1);
}

TEST_CASE("OrderBook - Cancel order", "[order_book]") {
    lob::OrderBook book;
    
    (void)book.add_order(1, lob::Side::Buy, 100, 10);
    REQUIRE(book.order_count() == 1);
    
    REQUIRE(book.cancel_order(1));
    REQUIRE(book.order_count() == 0);
    REQUIRE(book.get_order(1) == nullptr);
}

TEST_CASE("OrderBook - Modify order", "[order_book]") {
    lob::OrderBook book;
    
    (void)book.add_order(1, lob::Side::Buy, 100, 10);
    
    REQUIRE(book.modify_order(1, 105, 15));
    
    const auto* order = book.get_order(1);
    REQUIRE(order != nullptr);
    REQUIRE(order->price == 105);
    REQUIRE(order->quantity == 15);
    
    auto best_bid = book.best_bid();
    REQUIRE(best_bid.has_value());
    REQUIRE(*best_bid == 105);
}

TEST_CASE("OrderBook - Price-time priority", "[order_book]") {
    lob::OrderBook book;
    
    // Add multiple orders at same price
    (void)book.add_order(1, lob::Side::Buy, 100, 10);
    (void)book.add_order(2, lob::Side::Buy, 100, 5);
    (void)book.add_order(3, lob::Side::Buy, 100, 8);
    
    auto levels = book.get_levels(lob::Side::Buy, 1);
    REQUIRE(levels.size() == 1);
    REQUIRE(levels[0].first == 100);
    REQUIRE(levels[0].second == 23);  // Total quantity: 10 + 5 + 8
}

TEST_CASE("OrderBook - Market depth", "[order_book]") {
    lob::OrderBook book;
    
    (void)book.add_order(1, lob::Side::Buy, 100, 10);
    (void)book.add_order(2, lob::Side::Buy, 100, 5);
    (void)book.add_order(3, lob::Side::Buy, 99, 8);
    
    REQUIRE(book.depth_at_price(lob::Side::Buy, 100) == 15);
    REQUIRE(book.depth_at_price(lob::Side::Buy, 99) == 8);
    REQUIRE(book.depth_at_price(lob::Side::Buy, 98) == 0);
}


#include <limits>
TEST_CASE("Depth overflow is rejected without changing the book", "[order_book]") {
    lob::OrderBook book;
    const auto maximum = std::numeric_limits<lob::Quantity>::max();
    REQUIRE(book.add_order(1, lob::Side::Buy, 100, maximum));
    REQUIRE_FALSE(book.add_order(2, lob::Side::Buy, 100, 1));
    REQUIRE(book.add_order(2, lob::Side::Buy, 99, 1));
    REQUIRE_FALSE(book.modify_order(2, 100, 1));
    REQUIRE(book.get_order(2)->price == 99);
    REQUIRE(book.depth_at_price(lob::Side::Buy, 100) == maximum);
}

TEST_CASE("Spread avoids signed overflow", "[order_book]") {
    lob::OrderBook book;
    REQUIRE(book.add_order(1, lob::Side::Buy, std::numeric_limits<lob::Price>::min(), 1));
    REQUIRE(book.add_order(2, lob::Side::Sell, std::numeric_limits<lob::Price>::max(), 1));
    REQUIRE_FALSE(book.spread());
}
