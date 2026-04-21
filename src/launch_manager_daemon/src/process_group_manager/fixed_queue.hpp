/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef FIXED_QUEUE_HPP_INCLUDED
#define FIXED_QUEUE_HPP_INCLUDED

#include <array>
#include <cstddef>
#include <mutex>
#include <optional>

namespace score {

namespace lcm {

namespace internal {

/// @brief A thread-safe, pre-allocated, fixed-capacity queue supporting move-only types.
/// @tparam T The element type (may be move-only).
/// @tparam Capacity The maximum number of elements the queue can hold (pre-allocated at construction).
template <typename T, std::size_t Capacity>
class FixedQueue final {
   public:
    bool tryEnqueue(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ >= Capacity) {
            return false;
        }
        slots_[write_idx_] = std::move(item);
        write_idx_ = (write_idx_ + 1) % Capacity;
        ++count_;
        return true;
    }

    std::optional<T> tryDequeue() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0) {
            return std::nullopt;
        }
        auto item = std::move(slots_[read_idx_]);
        slots_[read_idx_].reset();
        read_idx_ = (read_idx_ + 1) % Capacity;
        --count_;
        return item;
    }

   private:
    std::array<std::optional<T>, Capacity> slots_{};
    std::mutex mutex_;
    std::size_t write_idx_{0};
    std::size_t read_idx_{0};
    std::size_t count_{0};
};

}  // namespace internal

}  // namespace lcm

}  // namespace score

#endif  // FIXED_QUEUE_HPP_INCLUDED
