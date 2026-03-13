/* SPDX-FileCopyrightText: 2025 LichtFeld Studio Authors
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include "core/logger.hpp"
#include <atomic>
#include <cstdint>
#include <musa_runtime.h>
#include <mutex>
#include <vector>

namespace lfs::core {

    // Stream-ordered deferred free queue. Uses CUDA events to recycle GPU memory
    // without blocking. Call process() periodically to free completed allocations.
    class DeferredFreeQueue {
    public:
        static constexpr size_t INITIAL_CAPACITY = 1024;
        static constexpr size_t PROCESS_BATCH_SIZE = 64;
        static constexpr size_t EVENT_POOL_SIZE = 256;

        using FreeCallback = void (*)(void* ptr, size_t size);

        static DeferredFreeQueue& instance() {
            static DeferredFreeQueue queue;
            return queue;
        }

        void shutdown() {
            bool expected = false;
            if (!shutdown_.compare_exchange_strong(expected, true))
                return;
            flush();
            cleanup_event_pool();
        }

        void defer_free(void* ptr, size_t size, musaStream_t stream, FreeCallback callback) {
            if (!ptr)
                return;
            if (shutdown_.load(std::memory_order_acquire)) {
                callback(ptr, size);
                return;
            }

            musaEvent_t event = acquire_event();
            if (!event) {
                musaStreamSynchronize(stream);
                callback(ptr, size);
                return;
            }

            musaError_t err = musaEventRecord(event, stream);
            if (err != musaSuccess) {
                release_event(event);
                musaStreamSynchronize(stream);
                callback(ptr, size);
                return;
            }

            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                pending_.push_back({ptr, size, event, callback});
                stats_.queued_count.fetch_add(1, std::memory_order_relaxed);
                stats_.queued_bytes.fetch_add(size, std::memory_order_relaxed);
            }
        }

        size_t process(size_t max_items = PROCESS_BATCH_SIZE) {
            std::vector<PendingFree> to_free;
            to_free.reserve(max_items > 0 ? max_items : PROCESS_BATCH_SIZE);

            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (pending_.empty())
                    return 0;

                size_t count = 0;
                size_t i = 0;
                while (i < pending_.size() && (max_items == 0 || count < max_items)) {
                    const auto& item = pending_[i];
                    const musaError_t err = musaEventQuery(item.event);
                    if (err == musaSuccess) {
                        to_free.push_back(item);
                        release_event(item.event);
                        pending_[i] = pending_.back();
                        pending_.pop_back();
                        ++count;
                    } else if (err == musaErrorNotReady) {
                        ++i;
                    } else {
                        LOG_WARN("musaEventQuery failed: {}", musaGetErrorString(err));
                        ++i;
                    }
                }
            }

            for (const auto& item : to_free) {
                item.callback(item.ptr, item.size);
                stats_.freed_count.fetch_add(1, std::memory_order_relaxed);
                stats_.freed_bytes.fetch_add(item.size, std::memory_order_relaxed);
                stats_.queued_bytes.fetch_sub(item.size, std::memory_order_relaxed);
            }

            return to_free.size();
        }

        void flush() {
            std::vector<PendingFree> to_free;

            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                to_free = std::move(pending_);
                pending_.clear();
            }

            musaDeviceSynchronize();

            for (const auto& item : to_free) {
                release_event(item.event);
                item.callback(item.ptr, item.size);
                stats_.freed_count.fetch_add(1, std::memory_order_relaxed);
                stats_.freed_bytes.fetch_add(item.size, std::memory_order_relaxed);
            }

            stats_.queued_bytes.store(0, std::memory_order_relaxed);
        }

        struct Stats {
            std::atomic<uint64_t> queued_count{0};
            std::atomic<uint64_t> freed_count{0};
            std::atomic<uint64_t> queued_bytes{0};
            std::atomic<uint64_t> freed_bytes{0};
        };

        const Stats& stats() const { return stats_; }

        size_t pending_count() const {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            return pending_.size();
        }

        DeferredFreeQueue(const DeferredFreeQueue&) = delete;
        DeferredFreeQueue& operator=(const DeferredFreeQueue&) = delete;

    private:
        struct PendingFree {
            void* ptr;
            size_t size;
            musaEvent_t event;
            FreeCallback callback;
        };

        DeferredFreeQueue() {
            pending_.reserve(INITIAL_CAPACITY);
            initialize_event_pool();
        }

        ~DeferredFreeQueue() {
            shutdown();
        }

        void initialize_event_pool() {
            std::lock_guard<std::mutex> lock(event_pool_mutex_);
            for (size_t i = 0; i < EVENT_POOL_SIZE; i++) {
                musaEvent_t event;
                musaError_t err = musaEventCreateWithFlags(&event, musaEventDisableTiming);
                if (err == musaSuccess) {
                    event_pool_.push_back(event);
                }
            }
            LOG_DEBUG("DeferredFreeQueue: created {} CUDA events", event_pool_.size());
        }

        void cleanup_event_pool() {
            std::lock_guard<std::mutex> lock(event_pool_mutex_);
            for (musaEvent_t event : event_pool_) {
                musaEventDestroy(event);
            }
            event_pool_.clear();
        }

        musaEvent_t acquire_event() {
            std::lock_guard<std::mutex> lock(event_pool_mutex_);
            if (event_pool_.empty()) {
                musaEvent_t event;
                if (musaEventCreateWithFlags(&event, musaEventDisableTiming) == musaSuccess) {
                    return event;
                }
                return nullptr;
            }
            musaEvent_t event = event_pool_.back();
            event_pool_.pop_back();
            return event;
        }

        void release_event(musaEvent_t event) {
            std::lock_guard<std::mutex> lock(event_pool_mutex_);
            if (event_pool_.size() < EVENT_POOL_SIZE * 2) {
                event_pool_.push_back(event);
            } else {
                musaEventDestroy(event);
            }
        }

        std::vector<PendingFree> pending_;
        mutable std::mutex queue_mutex_;

        std::vector<musaEvent_t> event_pool_;
        std::mutex event_pool_mutex_;

        std::atomic<bool> shutdown_{false};

        Stats stats_;
    };

} // namespace lfs::core
