#pragma once

#include <string>

#if defined(__x86_64__)
#include <emmintrin.h>
#endif

namespace dlsm::Thread {

void name(const std::string& name, std::size_t handle = 0);
std::string name(std::size_t handle = 0);

static constexpr std::size_t AllCPU = 0;
void affinity(std::size_t cpuid = AllCPU, std::size_t handle = 0);
std::size_t getaffinity(std::size_t handle = 0);

[[gnu::always_inline, gnu::hot]] inline void pause() noexcept {
#if defined(__x86_64__)
    _mm_pause();
#else
#warning "Unsupported CPU architecture, no special CPU pause instruction."
#endif
}

struct BusyPause {
    static inline void pause() noexcept { dlsm::Thread::pause(); }
};

struct NanoSleep {
    static void pause() noexcept;
};

template <typename T>
concept PauseConcept = requires(T c) {
    { c.pause() } noexcept;
};
static_assert(PauseConcept<BusyPause>);
static_assert(PauseConcept<NanoSleep>);

}  // namespace dlsm::Thread