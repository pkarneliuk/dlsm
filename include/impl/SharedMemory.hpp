#pragma once

#include <memory>
#include <new>
#include <span>
#include <string>

namespace dlsm {

struct SharedMemory {
    using UPtr = std::unique_ptr<SharedMemory>;

    virtual ~SharedMemory() = default;

    virtual bool owner() const noexcept = 0;
    virtual const std::string& name() const noexcept = 0;
    virtual void* address() const noexcept = 0;
    virtual std::size_t size() const noexcept = 0;

    template <typename T>
    T* data(std::size_t offset = 0) const noexcept {
        // C++23 std::start_lifetime_as_array
        return std::launder(reinterpret_cast<T*>(reinterpret_cast<char*>(address()) + offset));
    }

    template <typename T>
    T& reference(std::size_t offset = 0) const noexcept {
        return *data<T>(offset);
    }

    template <typename T>
    std::span<T> as() const noexcept {
        return {data<T>(), size() / sizeof(T)};
    }

    static UPtr create(const std::string& options);
};

}  // namespace dlsm
