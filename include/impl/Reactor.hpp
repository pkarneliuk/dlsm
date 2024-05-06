#pragma once

#include <zmq.h>

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <thread>

#include <iostream>
#include <map>

#include "impl/Str.hpp"

namespace dlsm {

namespace Payload {

template <typename T>
concept POD = std::is_trivially_destructible_v<T> && std::is_standard_layout_v<T> && sizeof(T) <= 1024;

template <typename C, typename T = typename C::value_type>
concept Contiguous = POD<T> && requires(C c) {
    { std::data(c) };
    { std::size(c) };
};

template <typename C, typename T = typename C::value_type>
concept Sequence = POD<T> && !Contiguous<C> && requires(C c) {
    { c.begin() };
    { c.end() };
};

}  // namespace Payload


// Using zmq_msg for direct read/write to ZMQ buffers
// Generic interface for Socket(Endpoint?)
// Block/non-Block Read/Write operations
// Public interface for Handler
// Add/Del handlers from Poller during handling for specific Read/Write operation
// Hide ZMQ behind interface
// Add zmq_socket_monitor with handler

class Reactor {
public:
    static_assert((ZMQ_VERSION_MAJOR == 4) && (ZMQ_VERSION_MINOR >= 3), "ZeroMQ 4.3+ is required");

    static inline void check(const bool success, std::string_view description = {}) {
        if (!success) [[unlikely]] {
            throw std::runtime_error(std::string{description} + " Error:" + zmq_strerror(zmq_errno()));
        }
    }

    const std::unique_ptr<void, void (*)(void*)> context_;

    struct Timers {
        void* timers_ = nullptr;

        Timers() : timers_{zmq_timers_new()} {}
        ~Timers() { zmq_timers_destroy(&timers_); }

        struct ITimer {
            using Ptr = std::unique_ptr<ITimer>;
            struct Handler {
                virtual ~Handler() = default;
                virtual void handler(ITimer& timer) = 0;
            };
            virtual ~ITimer() = default;
            virtual bool active() = 0;
            virtual void cancel() = 0;
            virtual void reset() = 0;
            virtual void set(std::chrono::milliseconds interval) = 0;
        };

        struct Timer : public ITimer {
            Timers& timers_;
            Handler& handler_;
            int id_ = -1;

            static void timer([[maybe_unused]]int timer_id, void* arg) {
                auto instance = reinterpret_cast<Timer*>(arg);
                instance->handler_.handler(*instance);
            }

            Timer(Timers& timers, Handler& handler, std::chrono::milliseconds interval)
            : timers_{timers}, handler_{handler} {
                id_ = zmq_timers_add(timers_.timers_, interval.count(), &timer, this);
            }
            ~Timer() {
                cancel();
            }
            bool active() override {
                return id_ != -1;
            }
            void cancel() override {
                if(id_ != -1) {
                    check(-1 != zmq_timers_cancel(timers_.timers_, id_), "zmq_timers_cancel()");
                    id_ = -1;
                }
            }
            void reset() override {
                check(-1 != zmq_timers_reset(timers_.timers_, id_), "zmq_timers_reset()");
            }
            void set(std::chrono::milliseconds interval) override {
                check(-1 != zmq_timers_set_interval(timers_.timers_, id_, interval.count()), "zmq_timers_set_interval()");
            }
        };

        ITimer::Ptr timer(Timer::Handler& handler, std::chrono::milliseconds interval) {
            return std::make_unique<Timer>(*this, handler, interval);
        }
        std::chrono::milliseconds timeout() {
            return std::chrono::milliseconds{zmq_timers_timeout(timers_)};
        }
        void execute() {
            check(-1 != zmq_timers_execute(timers_), "zmq_timers_execute()");
        }
    };


    Reactor(std::string_view options) : context_{zmq_ctx_new(), [](void* ptr) {
                       if (ptr) check(0 == zmq_ctx_term(ptr));
                   }} {
        check(nullptr != context_);
        static const auto opts = std::map<std::string_view, int>{
            // clang-format off
            {"io_threads",   ZMQ_IO_THREADS},
            {"max_sockets",  ZMQ_MAX_SOCKETS},
            {"sched_policy", ZMQ_THREAD_SCHED_POLICY},
            {"priority",     ZMQ_THREAD_PRIORITY},
            {"cpu_add",      ZMQ_THREAD_AFFINITY_CPU_ADD},
            {"cpu_rem",      ZMQ_THREAD_AFFINITY_CPU_REMOVE},
            {"ipv6",         ZMQ_IPV6},
            // clang-format on
        };

        // Parse options and try to set their values to context
        for (const auto& opt : dlsm::Str::ParseOpts(options)) try {
                check(0 == zmq_ctx_set(context_.get(), opts.at(opt.first), std::stoi(opt.second)));
            } catch (const std::exception& e) {
                throw std::invalid_argument("Unexpected ZMQ Context option:" + opt.first + "=" + opt.second +
                                            " with:" + e.what());
            }
    }
    ~Reactor() = default;


    using Data = std::span<std::byte>;
    struct Msg {
        zmq_msg_t msg_;
        Msg() {
            check(-1 != zmq_msg_init(&msg_), "zmq_msg_init()");
        }
        Msg(std::size_t length) {
            check(-1 != zmq_msg_init_size(&msg_, length), "zmq_msg_init_size()");
        }
        Msg(Data data) {
            check(-1 != zmq_msg_init_data(&msg_, data.data(), data.size_bytes(), nullptr, nullptr), "zmq_msg_init_data()");
        }
        Data data() {
            return Data{reinterpret_cast<std::byte*>(zmq_msg_data(&msg_)), zmq_msg_size(&msg_)};
        }
        operator Data() {
            return data();
        }
        ~Msg() {
            zmq_msg_close(&msg_);
        }
    };


    struct Endpoint {
        using Ptr = std::unique_ptr<Endpoint>;
        std::vector<std::string> subscriptions_;
        const int type_;
        const std::unique_ptr<void, void (*)(void*)> socket_;

        static int type(const dlsm::Str::ParseOpts& opts) {
            // clang-format off
            static const auto types = std::map<std::string_view, int> {
                {"pair", ZMQ_PAIR},
                {"pub", ZMQ_PUB}, {"sub", ZMQ_SUB},
                {"req", ZMQ_REQ}, {"rep", ZMQ_REP},
                {"dealer", ZMQ_DEALER}, {"router", ZMQ_ROUTER},
                {"pull", ZMQ_PULL}, {"push", ZMQ_PUSH},
                {"xpub", ZMQ_XPUB}, {"xsub", ZMQ_XSUB},
                {"stream", ZMQ_STREAM},
                {"server", ZMQ_SERVER}, {"client", ZMQ_CLIENT},
                {"radio", ZMQ_RADIO}, {"dish", ZMQ_DISH},
                {"gather", ZMQ_GATHER}, {"scatter", ZMQ_SCATTER},
                {"dgram", ZMQ_DGRAM}, {"peer", ZMQ_PEER},
                {"channel", ZMQ_CHANNEL},
            };
            // clang-format on
            return types.at(opts.get("type"));
        }
        Endpoint(Reactor& zmq, const dlsm::Str::ParseOpts& opts) : Endpoint(zmq, type(opts), opts) {}
        Endpoint(Reactor& zmq, int type, const dlsm::Str::ParseOpts& opts)
            : type_{type}, socket_{zmq_socket(zmq.context_.get(), type_), [](void* ptr) {
                                                 if (ptr) check(0 == zmq_close(ptr));
                                             }} {
            check(nullptr != socket_);

            const auto set = [&](auto parser, const std::map<std::string, int>& options) {
                for (const auto& opt : options) {
                    const auto& v = opts.get(opt.first);
                    try {
                        if (!v.empty()) {
                            auto value = parser(v);
                            check(0 == zmq_setsockopt(socket(), opt.second, &value, sizeof(value)));
                        }
                    } catch (const std::exception& e) {
                        throw std::invalid_argument("Unexpected ZMQ Endpoint option:" + opt.first + "=" + v +
                                                    " with:" + e.what());
                    }
                }
            };

            // clang-format off
            set([](const auto& str) { return std::stoi(str); }, {
                {"recv_buf_size",       ZMQ_RCVBUF},
                {"recv_hwm_msgs",       ZMQ_RCVHWM},
                {"recv_timeout_ms",     ZMQ_RCVTIMEO},
                {"send_buf_size",       ZMQ_SNDBUF},
                {"send_hwm_msgs",       ZMQ_SNDHWM},
                {"send_timeout_ms",     ZMQ_SNDTIMEO},
                {"connect_timeout_ms",  ZMQ_CONNECT_TIMEOUT},
                {"ipv6",                ZMQ_IPV6},
                //{"priority",            ZMQ_PRIORITY},
            });
            // clang-format on
            subscriptions_ = dlsm::Str::split(opts.get("subscribe", ""), "+");
            const auto& endpoint = opts.required("endpoint");
            switch (type_) {
                case ZMQ_PUB:
                    check(0 == zmq_bind(socket(), endpoint.c_str()), "Bind to '" + endpoint + "'");
                    break;
                case ZMQ_SUB:
                    check(0 == zmq_connect(socket(), endpoint.c_str()), "Connect to '" + endpoint + "'");
                    for (const auto& topic : subscriptions_) subscription<true>(topic);
                    if (auto delay_ms = std::atoi(opts.get("delay_ms", "10").c_str()); delay_ms != 0) {
                        // Give some time to establish subscriptions
                        std::this_thread::sleep_for(std::chrono::milliseconds{delay_ms});
                    }
                    break;
                // default:
                //     throw std::invalid_argument("Endpoint type=" + std::to_string(type_) + " is unsupported");
            }
        }
        ~Endpoint() noexcept(false) {
            std::string endpoint(256, '\0');
            std::size_t size = std::size(endpoint);
            check(0 == zmq_getsockopt(socket(), ZMQ_LAST_ENDPOINT, std::data(endpoint), &size));
            endpoint.resize(size);
            switch (type_) {
                case ZMQ_PUB:
                    check(0 == zmq_unbind(socket(), std::data(endpoint)), "unbind(" + endpoint + ") ");
                    break;
                case ZMQ_SUB:
                    for (const auto& topic : subscriptions_) subscription<false>(topic);
                    check(0 == zmq_disconnect(socket(), std::data(endpoint)), "disconnect(" + endpoint + ") ");
                    break;
                // default:
                //     throw std::invalid_argument("Endpoint type is unsupported in dtor");
            }
        }

        template <bool Enable>
        void subscription(const std::string& topic) {
            const auto [opt, msg] = Enable ? std::pair{ZMQ_SUBSCRIBE, "Subscribe to '" + topic + "' "}
                                           : std::pair{ZMQ_UNSUBSCRIBE, "Unsubscribe from '" + topic + "' "};
            check(0 == zmq_setsockopt(socket(), opt, std::data(topic), std::size(topic)), msg);
        }

        void* socket() const { return socket_.get(); }

        void send(Msg data) {
            if (const int ret = zmq_msg_send(&data.msg_, socket(), 0); -1 == ret) [[unlikely]] {
                check(false, "Send " + std::to_string(zmq_errno()));
            }
        }

        Msg recv() {
            Msg data;
            if (const int ret = zmq_msg_recv(&data.msg_, socket(), 0); -1 == ret) [[unlikely]] {
                check(false, "Recv " + std::to_string(zmq_errno()));
            }
            return data;
        }
    };


    struct Monitor : Endpoint {

        Monitor(Reactor& zmq, const dlsm::Str::ParseOpts& opts) : Endpoint{zmq, ZMQ_PAIR, opts} {}
    };

    Endpoint::Ptr endpoint(std::string_view options) {
        return std::make_unique<Endpoint>(*this, options);
    }

    struct Message {
        Endpoint::Ptr& endpoint_;

        auto send(const void* data, std::size_t length) {
            endpoint_->send(Data{reinterpret_cast<Data::element_type*>(const_cast<void*>(data)), length});
            return true;
        }
        template <dlsm::Payload::POD POD>
        auto send(const POD& v) {
            return send(&v, sizeof(v));
        }
        template <dlsm::Payload::Contiguous Contiguous>
        auto send(const Contiguous& c) {
            auto ret = send(std::data(c), std::size(c) * sizeof(typename Contiguous::value_type));
            return ret ? std::size(c) : 0;
        }
        template <dlsm::Payload::Sequence Sequence>
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

        auto recv(void* data, std::size_t length) {
            Msg msg = endpoint_->recv();
            auto d = msg.data();
            auto len = std::min(length, std::size(d));
            std::memcpy(data, std::data(d), len);
            return len;
        }
        template <dlsm::Payload::POD POD>
        auto recv(POD& v) {
            return recv(&v, sizeof(v));
        }
        template <dlsm::Payload::Contiguous Contiguous>
        auto recv(Contiguous& c) {
            Msg msg = endpoint_->recv();
            auto payload = msg.data();
            const auto n = payload.size_bytes() / sizeof(typename Contiguous::value_type);
            c.resize(n);
            std::memcpy(std::data(c), payload.data(), payload.size_bytes());
            return n;
        }
        template <dlsm::Payload::Sequence Sequence>
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




    struct Handler {
        virtual ~Handler() = default;

        virtual void* socket() = 0;
        virtual void onIn(Reactor& reactor) = 0;
        virtual void onOut(Reactor& reactor) = 0;
        virtual void onError(Reactor& reactor) = 0;
    };
    std::vector<zmq_pollitem_t> sockets_;
    std::vector<Handler*> actions_;
    Timers timers_;

    bool has(Handler& handler) const {
        const auto socket = handler.socket();
        for(const auto& i : sockets_) {
            if(i.socket == socket) return true;
        }
        return false;
    }
    void add(Handler& handler) {
        sockets_.emplace_back(zmq_pollitem_t{
            .socket = handler.socket(),
            .fd = 0,
            .events = ZMQ_POLLIN | ZMQ_POLLOUT | ZMQ_POLLERR,
            .revents = 0
            }
        );
        actions_.emplace_back(&handler);
    }
    void del(Handler& handler) {
        const auto nitems = std::size(actions_);
        for(std::size_t i = 0; i < nitems; ++i) {
            if(actions_[i] == &handler) {
                std::swap(sockets_[i], sockets_.back());
                std::swap(actions_[i], actions_.back());

                sockets_.resize(nitems - 1);
                actions_.resize(nitems - 1);
                break;
            }
        }
    }

    std::size_t once(const std::chrono::milliseconds timeout) {
        const auto nitems = std::size(sockets_);
        long timeout_ms = timeout.count();
        if(timeout_ms == -1) timeout_ms = timers_.timeout().count();
        const auto n =  zmq_poll(std::data(sockets_), static_cast<int>(nitems), timeout_ms);
        check(n != -1, "zmq_poll()");

        for(std::size_t i = 0; i < nitems; ++i) {
            if(sockets_[i].revents & ZMQ_POLLIN) {
                actions_[i]->onIn(*this);
            }
            if(sockets_[i].revents & ZMQ_POLLOUT) {
                actions_[i]->onOut(*this);
            }
            if(sockets_[i].revents & ZMQ_POLLERR) {
                actions_[i]->onError(*this);
            }
        }

        timers_.execute();
        return n;
    }



    // Pub pub(std::string_view options);
    // Sub sub(std::string_view options);
    // Poller poller();
};
}  // namespace dlsm