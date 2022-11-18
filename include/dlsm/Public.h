#pragma once
// Dummy header for testing formatting scripts

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

/// <summary>A public class.</summary>
struct Public {
    /// <summary>A search engine.</summary>
    int _member = 0;
    Public() = default;
    /// <summary>Set method.</summary>
    /// <param name="v">New value to be set.</param>
    /// <returns>A new value.</returns>
    int set(int v) { return _member = v; }
};
