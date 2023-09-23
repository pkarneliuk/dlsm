#pragma once

#include <memory>
#include <string_view>
#include <type_traits>

namespace dlsm {

template <typename T>
concept PayloadPOD = std::is_trivially_destructible_v<T> && std::is_standard_layout_v<T> && sizeof(T) <= 1024;

template <typename C, typename T = typename C::value_type>
concept PayloadContiguous = PayloadPOD<T> && requires(C c) {
    { std::data(c) };
    { std::size(c) };
};

template <typename C, typename T = typename C::value_type>
concept PayloadSequence = PayloadPOD<T> && !PayloadContiguous<C> && requires(C c) {
    { c.begin() };
    { c.end() };
};

struct Transport {
    struct Exception : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };
    struct Endpoint {
        using UPtr = std::unique_ptr<Endpoint>;
        virtual ~Endpoint() noexcept(false) = default;

        virtual bool send(const void* data, const std::size_t length) = 0;
        virtual bool recv(void* data, std::size_t& length) = 0;

        template <dlsm::PayloadPOD POD>
        bool send(const POD& v) {
            return send(&v, sizeof(v));
        }
        template <dlsm::PayloadContiguous Contiguous>
        bool send(const Contiguous& c) {
            return send(std::data(c), std::size(c) * sizeof(typename Contiguous::value_type));
        }
        template <dlsm::PayloadSequence Sequence>
        std::size_t send(const Sequence& c) {
            std::size_t i = 0;
            for (const auto& e : c) {
                if (send(&e, sizeof(e)))
                    ++i;
                else
                    break;
            }
            return i;
        }

        template <dlsm::PayloadPOD POD>
        bool recv(POD& v) {
            std::size_t size = sizeof(v);
            return recv(&v, size);
        }
        template <dlsm::PayloadContiguous Contiguous>
        std::size_t recv(Contiguous& c) {
            std::size_t size = std::size(c) * sizeof(typename Contiguous::value_type);
            return recv(std::data(c), size) ? size : 0UL;
        }
        template <dlsm::PayloadSequence Sequence>
        std::size_t recv(Sequence& c) {
            std::size_t size = sizeof(typename Sequence::value_type);
            std::size_t i = 0;
            for (auto& e : c) {
                if (recv(&e, size))
                    ++i;
                else
                    break;
            }
            return i;
        }
    };

    using UPtr = std::unique_ptr<Transport>;
    virtual ~Transport() noexcept(false) = default;

    virtual Endpoint::UPtr endpoint(const std::string_view& config) = 0;

    static UPtr create(const std::string_view& options);
};
}  // namespace dlsm