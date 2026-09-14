#include "matching_engine.hpp"
#include <print>  // C++23 std::print and std::println

int main() {
    std::println("=== Limit Order Book & Matching Engine Demo ===\n");
    
    // Create matching engine with trade callback
    lob::MatchingEngine engine([](const lob::Trade& trade) {
        std::println("   Trade executed: {} shares @ ${} (Buy: {}, Sell: {})",
                     trade.quantity, trade.price, trade.buy_order_id, trade.sell_order_id);
    });
    
    std::println("1. Adding sell orders to the book:");
    (void)engine.submit_order(1, lob::Side::Sell, 100, 10);
    (void)engine.submit_order(2, lob::Side::Sell, 101, 5);
    (void)engine.submit_order(3, lob::Side::Sell, 102, 8);
    std::println("   Added 3 sell orders\n");
    
    std::println("2. Adding buy orders:");
    auto status1 = engine.submit_order(10, lob::Side::Buy, 100, 5);
    std::println("   Buy order 10: {}", 
                 status1 == lob::OrderStatus::Filled ? "FILLED" : "PARTIAL");
    
    auto status2 = engine.submit_order(11, lob::Side::Buy, 101, 8);
    std::println("   Buy order 11: {}", 
                 status2 == lob::OrderStatus::Filled ? "FILLED" : "PARTIAL");
    std::println("");
    
    std::println("3. Order book state:");
    const auto& book = engine.get_order_book();
    
    auto best_bid = book.best_bid();
    auto best_ask = book.best_ask();
    
    // C++23: Use std::optional monadic operations
    if (best_bid) {
        std::println("   Best Bid: ${}", *best_bid);
    } else {
        std::println("   Best Bid: None");
    }
    
    if (best_ask) {
        std::println("   Best Ask: ${}", *best_ask);
    } else {
        std::println("   Best Ask: None");
    }
    
    if (auto spread = book.spread()) {
        std::println("   Spread: ${}", *spread);
    }
    
    std::println("\n4. Market depth (top 3 levels):");
    auto bid_levels = book.get_levels(lob::Side::Buy, 3);
    auto ask_levels = book.get_levels(lob::Side::Sell, 3);
    
    std::println("   Bids:");
    for (const auto& [price, qty] : bid_levels) {
        std::println("     ${} : {} shares", price, qty);
    }
    
    std::println("   Asks:");
    for (const auto& [price, qty] : ask_levels) {
        std::println("     ${} : {} shares", price, qty);
    }
    
    std::println("\n5. Total orders in book: {}", book.order_count());
    
    std::println("\n=== Demo Complete ===");
    
    return 0;
}

