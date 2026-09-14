#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <array>
#include <new>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace lob::allocator {

// Single-threaded slab allocator with reusable order storage
// Pre-allocates memory pools (slabs) and maintains a free list for reuse
template<typename T>
class SlabAllocator {
public:
    static constexpr std::size_t DEFAULT_SLAB_SIZE = 1024;
    static constexpr std::size_t ALIGNMENT = alignof(T) > alignof(std::max_align_t) ? alignof(T) : alignof(std::max_align_t);
    
    explicit SlabAllocator(std::size_t slab_size = DEFAULT_SLAB_SIZE)
        : slab_size_(slab_size)
        , free_list_(nullptr)
    {
        if (slab_size_ < align_size(sizeof(T))) throw std::invalid_argument("Slab must fit at least one object");
        // Pre-allocate first slab for immediate use
        if (!allocate_slab()) {
            throw std::bad_alloc();
        }
    }
    
    ~SlabAllocator() {
        for (auto* slab : slabs_) {
            std::free(slab);
        }
    }
    
    // Stable ownership: slab pointers must not be shared by a defaulted move.
    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;
    SlabAllocator(SlabAllocator&&) = delete;
    SlabAllocator& operator=(SlabAllocator&&) = delete;
    
    // Allocate object from slab or free list (O(1) operation)
    [[nodiscard]] T* allocate() noexcept {
        // First check free list for reusable memory
        if (free_list_) {
            auto* node = reinterpret_cast<FreeNode*>(free_list_);
            auto* obj = free_list_;
            free_list_ = reinterpret_cast<T*>(node->next_free);
            new (obj) T();  // Placement new to construct object
            return obj;
        }
        
        // Try to allocate from current slab
        if (current_offset_ + sizeof(T) <= current_slab_size_) {
            auto* ptr = reinterpret_cast<T*>(
                reinterpret_cast<std::byte*>(current_slab_) + current_offset_
            );
            current_offset_ += align_size(sizeof(T));
            new (ptr) T();
            return ptr;
        }
        
        // Current slab full, allocate new slab
        if (!allocate_slab()) {
            return nullptr;  // Out of memory
        }
        
        // Allocate from newly allocated slab
        auto* ptr = reinterpret_cast<T*>(current_slab_);
        current_offset_ = align_size(sizeof(T));
        new (ptr) T();
        return ptr;
    }
    
    // Deallocate object and add to free list (O(1) operation)
    // Uses object's memory to store free list pointer (requires sizeof(T) >= sizeof(FreeNode))
    void deallocate(T* ptr) noexcept {
        if (!ptr) return;
        
        ptr->~T();  // Destruct object
        // Reuse object memory to store free list pointer
        auto* node = reinterpret_cast<FreeNode*>(ptr);
        node->next_free = reinterpret_cast<FreeNode*>(free_list_);
        free_list_ = ptr;
    }
    
    struct Stats {
        std::size_t total_slabs;
        std::size_t slab_size;
        std::size_t objects_allocated;
        std::size_t objects_in_free_list;
    };
    
    [[nodiscard]] Stats get_stats() const noexcept {
        std::size_t free_count = 0;
        auto* current = free_list_;
        while (current) {
            ++free_count;
            auto* node = reinterpret_cast<FreeNode*>(current);
            current = reinterpret_cast<T*>(node->next_free);
        }
        
        return {
            .total_slabs = slabs_.size(),
            .slab_size = slab_size_,
            .objects_allocated = (slabs_.size() - 1) * (slab_size_ / align_size(sizeof(T)))
                                + current_offset_ / align_size(sizeof(T)),
            .objects_in_free_list = free_count
        };
    }
    
private:
    // Free list node structure - reuses object memory when deallocated
    struct FreeNode {
        FreeNode* next_free;
    };
    
    static_assert(std::is_nothrow_default_constructible_v<T>);
    static_assert(std::is_trivially_destructible_v<T>, "Pool owns storage for trivial objects only");

    // Ensure type T is large enough to store FreeNode pointer
    static_assert(sizeof(T) >= sizeof(FreeNode), 
                  "Type T must be at least as large as FreeNode");
    
    // Align size to ALIGNMENT boundary (power-of-2 alignment)
    static constexpr std::size_t align_size(std::size_t size) noexcept {
        return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }
    
    // Allocate a new slab of memory
    // TODO: Consider adding slab size growth strategy (e.g., exponential growth)
    [[nodiscard]] bool allocate_slab() noexcept {
        const std::size_t aligned_size = align_size(sizeof(T));
        const std::size_t objects_per_slab = slab_size_ / aligned_size;
        const std::size_t actual_slab_size = objects_per_slab * aligned_size;
        
        // Allocate aligned memory block
        void* slab = std::aligned_alloc(ALIGNMENT, actual_slab_size);
        if (!slab) {
            return false;
        }
        
        try { slabs_.push_back(slab); }
        catch (...) { std::free(slab); return false; }
        current_slab_ = slab;
        current_slab_size_ = actual_slab_size;
        current_offset_ = 0;
        return true;
    }
    
    std::size_t slab_size_;
    std::vector<void*> slabs_;
    void* current_slab_{nullptr};
    std::size_t current_slab_size_{0};
    std::size_t current_offset_{0};
    
    T* free_list_{nullptr};
};

} // namespace lob::allocator

