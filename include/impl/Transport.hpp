#pragma once

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>

namespace dlsm {

template <typename C, typename T = typename C::value_type>
concept DataSize = requires(C c) {
    { std::data(c) };
    { std::size(c) };
};

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

struct ZMQ;

template <typename Runtime>
class Transport {
public:
    using Data = std::span<std::byte>;
    struct Pub {
        class Impl;
        std::unique_ptr<Impl> p;

        ~Pub();
        Data loan(const std::size_t length);
        void publish(Data payload);
        void release(Data payload);

        bool send(const void* data, const std::size_t length) {
            if (auto payload = loan(length); !payload.empty()) {
                std::memcpy(payload.data(), data, length);
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
            Data sample_;

            Sample(Pub& p) : pub_{p}, sample_{p.loan(sizeof(T))} {}
            ~Sample() {
                if (!sample_.empty()) pub_.release(sample_);
            }

            operator bool() const { return !sample_.empty(); }
            T& value() { return *reinterpret_cast<T*>(sample_.data()); }
            void publish() {
                if (!sample_.empty()) {
                    pub_.publish(sample_);
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
        Data take();
        void release(Data payload);

        bool recv(void* data, std::size_t& length) {
            if (Data payload = take(); !payload.empty()) {
                std::memcpy(data, payload.data(), length);
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
            if (Data payload = take(); !payload.empty()) {
                const auto n = payload.size() / sizeof(typename Contiguous::value_type);
                c.resize(n);
                std::memcpy(std::data(c), payload.data(), n * sizeof(typename Contiguous::value_type));
                release(payload);
                return n;
            }
            return 0;
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
            Data sample_;

            Sample(Sub& s) : sub_{s}, sample_{sub_.take()} {}
            ~Sample() {
                if (!sample_.empty()) sub_.release(sample_);
            }

            operator bool() const { return !sample_.empty(); }
            const T& value() { return *reinterpret_cast<const T*>(sample_.data()); }
        };

        template <typename T>
        const Sample<T> take() {
            return {*this};
        }
    };

    struct Poller {
        class Impl;
        std::unique_ptr<Impl> p;


        struct Timer {
            using Ptr = std::unique_ptr<Timer>;
            struct Handler {
                virtual ~Handler() = default;
                virtual void handler(Timer& timer) = 0;
            };
            virtual ~Timer() = default;
            virtual bool active() = 0;
            virtual void cancel() = 0;
            virtual void reset() = 0;
            virtual void set(std::chrono::milliseconds interval) = 0;
        };

        ~Poller();

        Timer::Ptr timer(Timer::Handler& handler, std::chrono::milliseconds interval);
        std::size_t once(std::chrono::milliseconds timeout = 0);
    };

    const std::unique_ptr<Runtime> ptr_;

    Transport(std::string_view options);
    ~Transport();

    Pub pub(std::string_view options);
    Sub sub(std::string_view options);
    Poller poller();
};
}  // namespace dlsm