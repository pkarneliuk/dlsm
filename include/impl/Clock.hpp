#pragma once

#include <sys/time.h>

#include <cassert>
#include <chrono>
#include <concepts>
#include <ctime>

#if defined(__x86_64__)
#include <x86intrin.h>
namespace dlsm::Clock {
[[gnu::always_inline, gnu::hot]] inline std::uint64_t cputicks() noexcept { return __rdtsc(); }
}  // namespace dlsm::Clock
#else
#warning "Unsupported CPU architecture, no special CPU rdtsc() intrinsic."
namespace dlsm::Clock {
[[gnu::always_inline, gnu::hot]] inline std::uint64_t cputicks() noexcept { return 0; }
}  // namespace dlsm::Clock
#endif

namespace dlsm::Clock {

template <typename C>
concept Concept = requires(C c) {
    { C::timestamp() } -> std::same_as<std::chrono::nanoseconds>;
    { C::resolution() } -> std::same_as<std::chrono::nanoseconds>;
};

template <typename StdClock>
struct StdChrono {
    static std::chrono::nanoseconds timestamp() noexcept(noexcept(StdClock::now())) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(StdClock::now().time_since_epoch());
    }

    static std::chrono::nanoseconds resolution() noexcept {
        return std::chrono::nanoseconds(StdClock::period::num * 1'000'000'000LL / StdClock::period::den);
    }
};

using System = StdChrono<std::chrono::system_clock>;
using Steady = StdChrono<std::chrono::steady_clock>;
using HiRes = StdChrono<std::chrono::high_resolution_clock>;

template <::clockid_t TYPE>
struct ClockGetTime {
    static std::chrono::nanoseconds timestamp() noexcept {
        struct ::timespec tp{};
        [[maybe_unused]] int r = ::clock_gettime(TYPE, &tp);
        assert(r == 0);
        return TimespecToNano(tp);
    }

    static std::chrono::nanoseconds resolution() noexcept {
        struct ::timespec res{};
        [[maybe_unused]] int r = ::clock_getres(TYPE, &res);
        assert(r == 0);
        return TimespecToNano(res);
    }

private:
    [[gnu::always_inline, gnu::hot]] static constexpr std::chrono::nanoseconds TimespecToNano(
        const ::timespec& ts) noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds{ts.tv_sec}) +
               std::chrono::nanoseconds{ts.tv_nsec};
    }
};

using Realtime = ClockGetTime<CLOCK_REALTIME>;
using Monotonic = ClockGetTime<CLOCK_MONOTONIC>;
using RealCoarse = ClockGetTime<CLOCK_REALTIME_COARSE>;
using MonoCoarse = ClockGetTime<CLOCK_MONOTONIC_COARSE>;

struct GetTimeOfDay {
    static std::chrono::nanoseconds timestamp() noexcept {
        struct ::timeval tv{};
        [[maybe_unused]] int r = gettimeofday(&tv, nullptr);
        assert(r == 0);
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds{tv.tv_sec} +
                                                                    std::chrono::microseconds{tv.tv_usec});
    }

    static std::chrono::nanoseconds resolution() { return std::chrono::nanoseconds{1000}; }
};

}  // namespace dlsm::Clock
