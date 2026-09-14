#include "matching_engine.hpp"
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
std::uint64_t parse(std::string_view text, std::uint64_t maximum) {
    std::uint64_t value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value == 0 || value > maximum)
        throw std::invalid_argument("Arguments must be positive integers within the documented limits");
    return value;
}
}

// Deterministic complete book lifecycle. Perf measures ALL of this process,
// including setup, timestamps, validation, trade recording, and destruction.
int main(int argc, char** argv) {
    try {
        if (argc > 3) throw std::invalid_argument("Usage: clob_profile [batches=200000] [orders_per_batch=100]");
        const auto batches = argc > 1 ? parse(argv[1], 1000000) : 200000;
        const auto orders = argc > 2 ? parse(argv[2], 10000) : 100;
        lob::MatchingEngine engine;
        std::uint64_t checksum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (std::uint64_t batch = 0; batch < batches; ++batch) {
            // Alternate directions to exercise buy and sell matching equally.
            const auto maker_side = batch % 2 ? lob::Side::Buy : lob::Side::Sell;
            const auto taker_side = maker_side == lob::Side::Buy ? lob::Side::Sell : lob::Side::Buy;
            for (lob::OrderId id = 1; id <= orders; ++id) {
                if (engine.submit_order(id, maker_side, 100 + id % 10, 1) != lob::OrderStatus::New)
                    throw std::runtime_error("Resting order was not accepted");
            }
            const auto limit = taker_side == lob::Side::Buy ? 109 : 100;
            if (engine.submit_order(orders + 1, taker_side, limit, orders) != lob::OrderStatus::Filled)
                throw std::runtime_error("Sweep did not fill");
            const auto trades = engine.get_trades();
            if (trades.size() != orders || engine.get_order_book().order_count() != 0 ||
                engine.get_order_book().best_bid() || engine.get_order_book().best_ask())
                throw std::runtime_error("Unexpected trade count or residual liquidity");
            for (const auto& trade : trades) checksum += trade.quantity;
        }
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (checksum != batches * orders) throw std::runtime_error("Executed quantity mismatch");
        std::cout << "batches=" << batches << " orders_per_batch=" << orders
                  << " submissions=" << batches * (orders + 1)
                  << " trades=" << batches * orders << " checksum=" << checksum
                  << " workload_seconds=" << seconds << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
