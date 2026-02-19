#pragma once

#include "order.hpp"
#include <vector>
#include <stdexcept>
#include <cstddef>

namespace lob {

// Pre-allocated object pool with O(1) allocate/deallocate.
// Uses a free-list of indices into a contiguous memory block.
// No heap allocation after construction.
class OrderPool {
public:
    explicit OrderPool(std::size_t capacity)
        : orders_(capacity), free_stack_(capacity), capacity_(capacity), size_(0) {
        // Initialize free stack: all indices available
        for (std::size_t i = 0; i < capacity; ++i) {
            free_stack_[i] = capacity - 1 - i;  // top of stack = index 0
        }
        free_count_ = capacity;
    }

    // O(1) allocation from free list
    Order* allocate() {
        if (free_count_ == 0) {
            throw std::runtime_error("OrderPool exhausted");
        }
        std::size_t idx = free_stack_[--free_count_];
        orders_[idx].reset();
        ++size_;
        return &orders_[idx];
    }

    // O(1) deallocation back to free list
    void deallocate(Order* order) {
        std::size_t idx = static_cast<std::size_t>(order - orders_.data());
        free_stack_[free_count_++] = idx;
        --size_;
    }

    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }
    std::size_t available() const { return free_count_; }

private:
    std::vector<Order> orders_;          // contiguous order storage
    std::vector<std::size_t> free_stack_; // indices of free slots
    std::size_t capacity_;
    std::size_t size_;
    std::size_t free_count_;
};

}  // namespace lob
