#pragma once

#include <cstdint>
#include <cstring>
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

struct IOX;
struct ZMQ;

template <typename Runtime>
class Transport {
public:
    struct Pub {
        class Impl;
        std::unique_ptr<Impl> p;

        ~Pub();
        void* loan(const std::uint32_t payload);
        void publish(void* payload);
        void release(void* payload);

        bool send(const void* data, const std::size_t length) {
            if (void* payload = loan(static_cast<std::uint32_t>(length)); payload) {
                std::memcpy(payload, data, length);
                publish(payload);
                return true;
            }
            return false;
        }

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

        template <typename T>
        struct Sample {
            Pub& pub_;
            T* sample_;

            Sample(Pub& p) : pub_{p}, sample_{reinterpret_cast<T*>(p.loan(sizeof(T)))} {}
            ~Sample() {
                if (sample_) pub_.release(sample_);
            }

            operator bool() const { return sample_ != nullptr; }
            T& value() { return *sample_; }
            void publish() {
                if (sample_) {
                    pub_.publish(sample_);
                    sample_ = nullptr;
                }
            }
        };

        template <typename T>
        Sample<T> loan() {
            return {*this};
        }
    };

    struct Sub {
        class Impl;
        std::unique_ptr<Impl> p;

        ~Sub();
        const void* take();
        void release(const void* payload);

        bool recv(void* data, std::size_t& length) {
            if (const void* payload = take(); payload) {
                std::memcpy(data, payload, length);
                release(payload);
                return true;
            }
            return false;
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

        template <typename T>
        struct Sample {
            Sub& sub_;
            const T* sample_;

            Sample(Sub& s) : sub_{s}, sample_{reinterpret_cast<const T*>(sub_.take())} {}
            ~Sample() {
                if (sample_) sub_.release(sample_);
            }

            operator bool() const { return sample_ != nullptr; }
            const T& value() { return *sample_; }
        };

        template <typename T>
        const Sample<T> take() {
            return {*this};
        }
    };

    const std::unique_ptr<Runtime> ptr_;

    Transport(std::string_view options);
    ~Transport();

    Pub pub(std::string_view options);
    Sub sub(std::string_view options);
};
}  // namespace dlsm