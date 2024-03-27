#pragma once

#include <bitset>
#include <string>
#include <vector>

#if defined(__x86_64__)
#include <emmintrin.h>
#endif

namespace dlsm::Thread {

void name(const std::string& name, std::size_t handle = 0);
std::string name(std::size_t handle = 0);

static constexpr std::size_t MaskSize = 64;
struct Mask : public std::bitset<MaskSize> {
    Mask() = default;
    Mask(const std::vector<std::size_t>& affinity);
    operator std::vector<std::size_t>() const;
    explicit operator bool() const { return count(); }

    std::string to_string() const;
    Mask at(std::size_t index) const;
    Mask extract(std::size_t n = 1);  // Extract n least significant bits and return them
};

static_assert(sizeof(Mask) == sizeof(unsigned long long));

void affinity(Mask mask, std::size_t handle = 0);
[[nodiscard]] Mask affinity(std::size_t handle = 0);
static const Mask AllCPU = affinity();
// The number of processors currently online (available)
Mask AllAvailableCPU();

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