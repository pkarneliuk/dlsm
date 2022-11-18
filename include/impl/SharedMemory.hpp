#pragma once

#include <memory>
#include <string>

namespace dlsm {

struct SharedMemory {
    using UPtr = std::unique_ptr<SharedMemory>;

    virtual ~SharedMemory() = default;

    virtual const std::string& name() const noexcept = 0;
    virtual void* address() const noexcept = 0;
    virtual std::size_t size() const noexcept = 0;

    static UPtr create(const std::string& options);
};

}  // namespace dlsm
