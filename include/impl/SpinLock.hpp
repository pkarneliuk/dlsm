#pragma once

#include <atomic>
#include <concepts>
#include <mutex>

#include "impl/Thread.hpp"

namespace dlsm::SpinLock {

// Test-and-Set SpinLock
struct TASSLock {
    std::atomic<bool> lock_ = {false};

    inline bool try_lock() noexcept { return !lock_.exchange(true); }

    inline void lock() noexcept {
        while (lock_.exchange(true))
            ;
    }

    inline void unlock() noexcept { lock_.store(false); }
};

// Test and Test-and-Set SpinLock
struct TTSSLock {
    std::atomic<bool> lock_ = {false};

    inline bool try_lock() noexcept {
        // First do a relaxed load to check if lock is free in order to prevent
        // unnecessary cache misses if someone does while(!try_lock())
        return !lock_.load(std::memory_order_relaxed) && !lock_.exchange(true, std::memory_order_acquire);
    }

    inline void lock() noexcept {
        for (;;) {
            // Optimistically assume the lock is free on the first try
            if (!lock_.exchange(true, std::memory_order_acquire)) {
                return;
            }
            // Wait for lock to be released without generating cache misses
            while (lock_.load(std::memory_order_relaxed)) {
                dlsm::Thread::pause();
            }
        }
    }

    inline void unlock() noexcept { lock_.store(false, std::memory_order_release); }
};

template <typename Impl>
concept Concept = requires(Impl s) {
    { s.try_lock() } noexcept -> std::same_as<bool>;
    { s.lock() } noexcept -> std::same_as<void>;
    { s.unlock() } noexcept -> std::same_as<void>;
};
static_assert(Concept<TASSLock>);
static_assert(Concept<TTSSLock>);

using StdMutex = std::mutex;
using SpinLock = TTSSLock;

}  // namespace dlsm::SpinLock