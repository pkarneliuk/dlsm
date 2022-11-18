#pragma once

#include <memory>
#include <string>

namespace dlsm {
struct Transport {
    struct Endpoint {
        using UPtr = std::unique_ptr<Endpoint>;
        virtual ~Endpoint() = default;
    };

    using UPtr = std::unique_ptr<Transport>;
    virtual ~Transport() = default;

    virtual Endpoint::UPtr CreateEndpoint(const std::string& config) = 0;

    static UPtr Create(const std::string& options);
};
}  // namespace dlsm