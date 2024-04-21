#pragma once

#include <bit>
#include <format>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include "impl/Str.hpp"

namespace dlsm::Memory {

constexpr std::size_t alignmentOf(const void* address) noexcept {
    if (address == nullptr) return 0;
    return 1 << std::countr_zero(std::bit_cast<std::uintptr_t>(address));
}

template <typename T>
constexpr void checkAlignment(const T* value, const std::size_t expected = alignof(std::decay<T>)) {
    if (!std::has_single_bit(expected))
        throw std::invalid_argument{std::format("Alignment must be power of two:{}", expected)};
    const auto address = std::bit_cast<const void*>(value);
    const auto alignment = alignmentOf(address);
    if (alignment < expected)
        throw std::runtime_error{std::format("Adderss: {} (alignment:{}) of '{}' is not aligned to:{}", address,
                                             alignment, dlsm::Str::typenameOf<T>(), expected)};
}

}  // namespace dlsm::Memory
