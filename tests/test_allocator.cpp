#include <catch2/catch_test_macros.hpp>
#include "allocator/slab_allocator.hpp"
#include "types.hpp"

TEST_CASE("SlabAllocator - Basic allocation", "[allocator]") {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    auto* order1 = allocator.allocate();
    REQUIRE(order1 != nullptr);
    
    auto* order2 = allocator.allocate();
    REQUIRE(order2 != nullptr);
    REQUIRE(order1 != order2);
    
    allocator.deallocate(order1);
    allocator.deallocate(order2);
}

TEST_CASE("SlabAllocator - Reuse from free list", "[allocator]") {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    auto* order1 = allocator.allocate();
    REQUIRE(order1 != nullptr);
    
    allocator.deallocate(order1);
    
    auto* order2 = allocator.allocate();
    REQUIRE(order2 != nullptr);
    // Should reuse the same memory
    REQUIRE(order1 == order2);
}

TEST_CASE("SlabAllocator - Statistics", "[allocator]") {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    auto stats1 = allocator.get_stats();
    REQUIRE(stats1.total_slabs >= 1);
    
    std::vector<lob::Order*> orders;
    for (int i = 0; i < 10; ++i) {
        orders.push_back(allocator.allocate());
    }
    
    auto stats2 = allocator.get_stats();
    REQUIRE(stats2.objects_allocated >= 10);
    
    for (auto* order : orders) {
        allocator.deallocate(order);
    }
    
    auto stats3 = allocator.get_stats();
    REQUIRE(stats3.objects_in_free_list >= 10);
}


#include <type_traits>
#include <stdexcept>

TEST_CASE("Allocator ownership cannot be copied or moved", "[allocator]") {
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<lob::allocator::SlabAllocator<lob::Order>>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<lob::allocator::SlabAllocator<lob::Order>>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<lob::allocator::SlabAllocator<lob::Order>>);
}

TEST_CASE("Allocator rejects undersized slabs and supports over-aligned types", "[allocator]") {
    REQUIRE_THROWS_AS(lob::allocator::SlabAllocator<lob::Order>(1), std::invalid_argument);
    struct alignas(64) Aligned { char bytes[64]{}; };
    lob::allocator::SlabAllocator<Aligned> allocator(128);
    std::vector<Aligned*> values;
    for (int i = 0; i < 10; ++i) {
        auto* p = allocator.allocate();
        REQUIRE(p != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(p) % 64 == 0);
        values.push_back(p);
    }
    for (auto* p : values) allocator.deallocate(p);
    REQUIRE(allocator.get_stats().objects_in_free_list == 10);
}

TEST_CASE("Allocator counts slots correctly across slabs with unused tail bytes", "[allocator]") {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    std::vector<lob::Order*> orders;
    for (int i = 0; i < 100; ++i) orders.push_back(allocator.allocate());
    REQUIRE(allocator.get_stats().objects_allocated == 100);
    for (auto* order : orders) allocator.deallocate(order);
    REQUIRE(allocator.get_stats().objects_in_free_list == 100);
}
