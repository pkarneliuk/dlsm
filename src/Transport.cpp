#include "impl/Transport.hpp"

#include <zmq.h>

#include <map>
#include <memory>
#include <thread>
#include <utility>

#include "impl/Str.hpp"

namespace dlsm {

struct ZMQ {
    static_assert((ZMQ_VERSION_MAJOR == 4) && (ZMQ_VERSION_MINOR >= 3), "ZeroMQ 4.3+ is required");

    static inline void check(const bool success, std::string_view description = {}) {
        if (!success) [[unlikely]] {
            throw std::runtime_error(std::string{description} + " Error:" + zmq_strerror(zmq_errno()));
        }
    }

    const std::unique_ptr<void, void (*)(void*)> context_;

    ZMQ(std::string_view options)
        : context_{zmq_ctx_new(), [](void* ptr) {
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

    using Data = Transport<ZMQ>::Data;


    struct Msg {
        zmq_msg_t msg_;
        Msg() {
            zmq_msg_init(&msg_);
        }
        Data data() {
            return Data{reinterpret_cast<std::byte*>(zmq_msg_data(&msg_)), zmq_msg_size(&msg_)};
        }
        ~Msg() {
            zmq_msg_close(&msg_);
        }
    };


    struct Endpoint {
        zmq_msg_t msg_;
        std::vector<std::string> subscriptions_;
        const int type_;
        const std::unique_ptr<void, void (*)(void*)> socket_;

        Endpoint(ZMQ& zmq, int type, const dlsm::Str::ParseOpts& opts)
            : msg_{{}}, type_{type}, socket_{zmq_socket(zmq.context_.get(), type_), [](void* ptr) {
                                                 if (ptr) check(0 == zmq_close(ptr));
                                             }} {
            check(nullptr != socket_);

            const auto set = [&](auto parser, const std::map<std::string, int>& options) {
                for (const auto& opt : options) {
                    const auto& v = opts.get(opt.first);
                    try {
                        if (!v.empty()) {
                            auto value = parser(v);
                            check(0 == zmq_setsockopt(socket_.get(), opt.second, &value, sizeof(value)));
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
                    check(0 == zmq_bind(socket_.get(), endpoint.c_str()), "Bind to '" + endpoint + "'");
                    break;
                case ZMQ_SUB:
                    check(0 == zmq_connect(socket_.get(), endpoint.c_str()), "Connect to '" + endpoint + "'");
                    for (const auto& topic : subscriptions_) subscription<true>(topic);
                    if (auto delay_ms = std::atoi(opts.get("delay_ms", "10").c_str()); delay_ms != 0) {
                        // Give some time to establish subscriptions
                        std::this_thread::sleep_for(std::chrono::milliseconds{delay_ms});
                    }
                    break;
                default:
                    throw std::invalid_argument("Endpoint type=" + std::to_string(type_) + " is unsupported");
            }
        }
        ~Endpoint() noexcept(false) {
            std::string endpoint(256, '\0');
            std::size_t size = std::size(endpoint);
            check(0 == zmq_getsockopt(socket_.get(), ZMQ_LAST_ENDPOINT, std::data(endpoint), &size));
            endpoint.resize(size);
            switch (type_) {
                case ZMQ_PUB:
                    check(0 == zmq_unbind(socket_.get(), std::data(endpoint)), "unbind(" + endpoint + ") ");
                    break;
                case ZMQ_SUB:
                    for (const auto& topic : subscriptions_) subscription<false>(topic);
                    check(0 == zmq_disconnect(socket_.get(), std::data(endpoint)), "disconnect(" + endpoint + ") ");
                    break;
                default:
                    throw std::invalid_argument("Endpoint type is unsupported in dtor");
            }
        }

        template <bool Enable>
        void subscription(const std::string& topic) {
            const auto [opt, msg] = Enable ? std::pair{ZMQ_SUBSCRIBE, "Subscribe to '" + topic + "' "}
                                           : std::pair{ZMQ_UNSUBSCRIBE, "Unsubscribe from '" + topic + "' "};
            check(0 == zmq_setsockopt(socket_.get(), opt, std::data(topic), std::size(topic)), msg);
        }
    };

    class Pub : private Endpoint {
    public:
        Pub(ZMQ& zmq, std::string_view config) : Endpoint{zmq, ZMQ_PUB, dlsm::Str::ParseOpts{config}} {}

        inline Data loan(const std::size_t length) {
            if (int ret = zmq_msg_init_size(&msg_, length); ret != 0) return Data{};
            return Data{reinterpret_cast<std::byte*>(zmq_msg_data(&msg_)), zmq_msg_size(&msg_)};
        }
        inline void publish([[maybe_unused]] Data payload) {
            const int ret = zmq_msg_send(&msg_, socket_.get(), 0);
            if (-1 == ret) [[unlikely]] {
                const auto code = zmq_errno();
                check(false, "Send " + std::to_string(code) + " ret: " + std::to_string(ret));
            }
        }
        inline void release([[maybe_unused]] Data payload) { zmq_msg_close(&msg_); }
    };

    class Sub : private Endpoint {
    public:
        Sub(ZMQ& zmq, std::string_view config) : Endpoint{zmq, ZMQ_SUB, dlsm::Str::ParseOpts{config}} {}

        Data take() {
            zmq_msg_init(&msg_);
            if (int ret = zmq_msg_recv(&msg_, socket_.get(), 0); ret == -1) {
                zmq_msg_close(&msg_);
                const auto code = zmq_errno();
                check(false, "Recv " + std::to_string(code) + " ret: " + std::to_string(ret));
            }

            return Data{reinterpret_cast<std::byte*>(zmq_msg_data(&msg_)), zmq_msg_size(&msg_)};
        }
        void release([[maybe_unused]] Data payload) { zmq_msg_close(&msg_); }
    };

    struct Poller {

        struct Handler {
            virtual ~Handler() = default;

            virtual void* socket() = 0;
            virtual void onIn(Poller& reactor) = 0;
            virtual void onOut(Poller& reactor) = 0;
            virtual void onError(Poller& reactor) = 0;
        };
        std::vector<zmq_pollitem_t> sockets_;
        std::vector<Handler*> actions_;
        void* timers_ = nullptr;

        Poller() : timers_{zmq_timers_new()} {}
        ~Poller() {
            zmq_timers_destroy(&timers_);
        }


        struct Timer : public Transport<ZMQ>::Poller::Timer {
            Poller& poller_;
            Handler& handler_;
            int id_ = -1;

            static void timer([[maybe_unused]]int timer_id, void* arg) {
                auto instance = reinterpret_cast<Timer*>(arg);
                instance->handler_.handler(*instance);
            }

            Timer(Poller& poller, Handler& handler, std::chrono::milliseconds interval)
            : poller_{poller}, handler_{handler} {
                id_ = zmq_timers_add(poller.timers_, interval.count(), &timer, this);
            }
            ~Timer() {
                cancel();
            }
            bool active() override {
                return id_ != -1;
            }
            void cancel() override {
                if(id_ != -1) {
                    check(-1 != zmq_timers_cancel(poller_.timers_, id_), "zmq_timers_cancel()");
                    id_ = -1;
                }
            }
            void reset() override {
                check(-1 != zmq_timers_reset(poller_.timers_, id_), "zmq_timers_reset()");
            }
            void set(std::chrono::milliseconds interval) override {
                check(-1 != zmq_timers_set_interval(poller_.timers_, id_, interval.count()), "zmq_timers_set_interval()");
            }
        };

        Timer timer(typename Transport<ZMQ>::Poller::Timer::Handler& handler, std::chrono::milliseconds interval) {
            return Timer{*this, handler, interval};
        }

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
            if(timeout_ms == -1) timeout_ms = zmq_timers_timeout(timers_);
            const auto n =  zmq_poll(std::data(sockets_), static_cast<int>(nitems), timeout_ms);
            check(n != -1, "zmq_poll()");

            for(std::size_t i = 0; i < nitems; ++i) {
                if(sockets_[i].revents & ZMQ_POLLIN) {

                    Msg msg;
                    actions_[i]->onIn(*this);
                }
                if(sockets_[i].revents & ZMQ_POLLOUT) {
                    actions_[i]->onOut(*this);
                }
                if(sockets_[i].revents & ZMQ_POLLERR) {
                    actions_[i]->onError(*this);
                }
            }

            check(-1 != zmq_timers_execute(timers_), "zmq_timers_execute()");

            return n;
        }

    };
};

template <typename Runtime>
Transport<Runtime>::Transport(std::string_view options) : ptr_{std::make_unique<Runtime>(options)} {}

template <typename Runtime>
Transport<Runtime>::~Transport() = default;

template <typename Runtime>
typename Transport<Runtime>::Pub Transport<Runtime>::pub(std::string_view options) {
    return {std::make_unique<typename Transport<Runtime>::Pub::Impl>(*ptr_, options)};
}

template <typename Runtime>
class Transport<Runtime>::Pub::Impl : public Runtime::Pub {
    using Runtime::Pub::Pub;
};

template <typename Runtime>
Transport<Runtime>::Pub::~Pub() = default;

template <typename Runtime>
Transport<Runtime>::Data Transport<Runtime>::Pub::loan(const std::size_t length) {
    return p->loan(length);
}

template <typename Runtime>
void Transport<Runtime>::Pub::publish(Data payload) {
    p->publish(payload);
}

template <typename Runtime>
void Transport<Runtime>::Pub::release(Data payload) {
    p->release(payload);
}

template <typename Runtime>
typename Transport<Runtime>::Sub Transport<Runtime>::sub(std::string_view options) {
    return {std::make_unique<typename Transport<Runtime>::Sub::Impl>(*ptr_, options)};
}

template <typename Runtime>
class Transport<Runtime>::Sub::Impl : public Runtime::Sub {
    using Runtime::Sub::Sub;
};

template <typename Runtime>
Transport<Runtime>::Sub::~Sub() = default;

template <typename Runtime>
Transport<Runtime>::Data Transport<Runtime>::Sub::take() {
    return p->take();
}

template <typename Runtime>
void Transport<Runtime>::Sub::release(Data payload) {
    p->release(payload);
}

template <typename Runtime>
class Transport<Runtime>::Poller::Impl : public Runtime::Poller {
    using Runtime::Poller::Poller;
};

template <typename Runtime>
Transport<Runtime>::Poller::~Poller() = default;

template <typename Runtime>
typename Transport<Runtime>::Poller Transport<Runtime>::poller() {
    return {std::make_unique<typename Transport<Runtime>::Poller::Impl>()};
}

template <typename Runtime>
Transport<Runtime>::Poller::Timer::Ptr Transport<Runtime>::Poller::timer(Timer::Handler& handler, std::chrono::milliseconds interval) {
    return std::make_unique<ZMQ::Poller::Timer>(*p, handler, interval);
}

template <typename Runtime>
std::size_t Transport<Runtime>::Poller::once(std::chrono::milliseconds timeout) {
    return p->once(timeout);
}


template class Transport<ZMQ>;

}  // namespace dlsm