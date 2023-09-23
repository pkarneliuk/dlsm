#pragma once

#include <string>

#if defined(__x86_64__)
#include <emmintrin.h>
namespace dlsm::Thread {
[[gnu::always_inline, gnu::hot]] inline void pause() noexcept { _mm_pause(); }
}  // namespace dlsm::Thread
#else
#warning "Unsupported CPU architecture, no special CPU pause instruction."
namespace dlsm::Thread {
[[gnu::always_inline, gnu::hot]] inline void pause() noexcept {}
}  // namespace dlsm::Thread
#endif

namespace dlsm::Thread {

void name(const std::string& name, std::size_t native_handle = 0);
std::string name(std::size_t native_handle = 0);

static constexpr std::size_t AllCPU = 0xFFFF;
void affinity(std::size_t cpuid = AllCPU, std::size_t native_handle = 0);

template <typename T>
concept Yield = requires(T c) { c.pause(); };

struct BusyPause {
    static inline void pause() noexcept { dlsm::Thread::pause(); }
};

struct NanoSleep {
    static void pause() noexcept;
};

}  // namespace dlsm::Thread