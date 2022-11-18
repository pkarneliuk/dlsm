#pragma once
// Dummy header for testing formatting scripts

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

#if defined(dlsmRuntime_EXPORTS)
#define DLSM_PUBLIC __attribute__((visibility("default")))
#else
#define DLSM_PUBLIC
#endif

namespace dlsm {
/// <summary>A public class.</summary>
struct DLSM_PUBLIC Public {
    /// <summary>A search engine.</summary>
    int _member = 0;
    Public() = default;
    /// <summary>Set method.</summary>
    /// <param name="v">New value to be set.</param>
    /// <returns>A new value.</returns>
    int set(int v);
    /// <summary>Get method.</summary>
    /// <returns>A current value.</returns>
    int get();
};

}  // namespace dlsm
