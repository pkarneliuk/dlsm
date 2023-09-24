#include "impl/Transport.hpp"

#include <zmq.h>

#include <map>
#include <memory>
#include <thread>

#include "impl/Str.hpp"

namespace {
class ZMQ final : public dlsm::Transport {
    static_assert((ZMQ_VERSION_MAJOR == 4) && (ZMQ_VERSION_MINOR >= 3), "ZeroMQ 4.3+ is required");

    static inline void check(const bool success, std::string_view description = {}) {
        if (!success) [[unlikely]] {
            throw Transport::Exception(std::string{description} + " Error:" + zmq_strerror(zmq_errno()));
        }
    }

    const std::unique_ptr<void, void (*)(void*)> context_;

public:
    ZMQ(const std::string_view& options)
        : context_{zmq_ctx_new(), [](void* ptr) {
                       if (ptr) check(0 == zmq_ctx_term(ptr));
                   }} {
        check(nullptr != context_);
        // clang-format off
        const auto opts = std::map<std::string_view, int>{
            {"io_threads",   ZMQ_IO_THREADS},
            {"max_sockets",  ZMQ_MAX_SOCKETS},
            {"sched_policy", ZMQ_THREAD_SCHED_POLICY},
            {"priority",     ZMQ_THREAD_PRIORITY},
            {"cpu_add",      ZMQ_THREAD_AFFINITY_CPU_ADD},
            {"cpu_rem",      ZMQ_THREAD_AFFINITY_CPU_REMOVE},
            {"ipv6",         ZMQ_IPV6},
        };
        // clang-format on

        // Parse options and try to set their values to context
        for (const auto& opt : dlsm::Str::ParseOpts(options)) try {
                check(0 == zmq_ctx_set(context_.get(), opts.at(opt.first), std::stoi(opt.second)));
            } catch (const std::exception& e) {
                throw std::invalid_argument("Unexpected ZMQ Context option:" + opt.first + "=" + opt.second +
                                            " with:" + e.what());
            }
    }

    ~ZMQ() noexcept(false) override = default;

    class Endpoint final : public dlsm::Transport::Endpoint {
        dlsm::Str::ParseOpts opts_;
        const int type_;
        const std::unique_ptr<void, void (*)(void*)> socket_;

        static int getType(const dlsm::Str::ParseOpts& opts) {
            static const auto types = std::map<std::string_view, int>{
                // clang-format off
                {"req", ZMQ_REQ},   {"rep", ZMQ_REP},
                {"pub", ZMQ_PUB},   {"sub", ZMQ_SUB},
                {"xpub", ZMQ_XPUB}, {"xsub", ZMQ_XSUB},
                {"push", ZMQ_PUSH}, {"pull", ZMQ_PULL},
                {"pair", ZMQ_PAIR},
                {"stream", ZMQ_STREAM},
                // clang-format on
            };
            return types.at(opts.get("type"));
        }

    public:
        Endpoint(ZMQ& zmq, const std::string_view& config) : Endpoint(zmq, dlsm::Str::ParseOpts(config)) {}
        Endpoint(ZMQ& zmq, dlsm::Str::ParseOpts opts)
            : opts_{std::move(opts)},
              type_{getType(opts_)},
              socket_{zmq_socket(zmq.context_.get(), type_), [](void* ptr) {
                          if (ptr) check(0 == zmq_close(ptr));
                      }} {
            check(nullptr != socket_);

            const auto set = [&](auto parser, const std::map<std::string, int>& options) {
                for (const auto& opt : options) {
                    const auto& v = opts_.get(opt.first);
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
            const auto& endpoint = opts_.required("endpoint");
            switch (type_) {
                case ZMQ_PUB:
                    check(0 == zmq_bind(socket_.get(), endpoint.c_str()), "Bind to '" + endpoint + "'");
                    break;
                case ZMQ_SUB:
                    check(0 == zmq_connect(socket_.get(), endpoint.c_str()), "Connect to '" + endpoint + "'");
                    for (const auto& topic : subscribtions()) subscription<true>(topic);
                    if (auto delay_ms = std::atoi(opts_.get("delay_ms", "10").c_str()); delay_ms != 0) {
                        // Give some time to establish subscriptions
                        std::this_thread::sleep_for(std::chrono::milliseconds{delay_ms});
                    }
                    break;
                default:
                    throw std::invalid_argument("Endpoint type=" + opts_.required("type") + " is unsupported");
            }
        }
        ~Endpoint() noexcept(false) override {
            std::string endpoint(256, '\0');
            std::size_t size = std::size(endpoint);
            check(0 == zmq_getsockopt(socket_.get(), ZMQ_LAST_ENDPOINT, std::data(endpoint), &size));
            endpoint.resize(size);
            switch (type_) {
                case ZMQ_PUB:
                    check(0 == zmq_unbind(socket_.get(), std::data(endpoint)), "unbind(" + endpoint + ") ");
                    break;
                case ZMQ_SUB:
                    for (const auto& topic : subscribtions()) subscription<false>(topic);
                    check(0 == zmq_disconnect(socket_.get(), std::data(endpoint)), "disconnect(" + endpoint + ") ");
                    break;
                default:
                    throw std::invalid_argument("Endpoint type is unsupported in dtor");
            }
        }

        bool send(const void* data, const std::size_t length) override {
            const int ret = zmq_send(socket_.get(), data, length, 0 /*ZMQ_SNDMORE*/);
            if (-1 == ret) [[unlikely]] {
                const auto code = zmq_errno();
                if (EAGAIN == code || EINTR == code) [[likely]]
                    return false;
                else [[unlikely]]
                    check(false, "Send");
            }
            const auto size = static_cast<std::size_t>(ret);
            return size == length;
        }
        bool recv(void* data, std::size_t& length) override {
            const int ret = zmq_recv(socket_.get(), data, length, 0 /*ZMQ_DONTWAIT*/);
            if (-1 == ret) [[unlikely]] {
                const auto code = zmq_errno();
                if (EAGAIN == code || EINTR == code) [[likely]]
                    return false;
                else [[unlikely]]
                    check(false, "Recv");
            }
            length = static_cast<std::size_t>(ret);
            return true;
        }

        template <bool Enable>
        void subscription(const std::string& topic) {
            const auto [opt, msg] = Enable ? std::pair{ZMQ_SUBSCRIBE, "Subscribe to '" + topic + "' "}
                                           : std::pair{ZMQ_UNSUBSCRIBE, "Unsubscribe from '" + topic + "' "};
            check(0 == zmq_setsockopt(socket_.get(), opt, std::data(topic), std::size(topic)), msg);
        }

        std::vector<std::string> subscribtions() const { return dlsm::Str::split(opts_.get("subscribe", ""), "+"); }
    };

    Endpoint::UPtr endpoint(const std::string_view& config) override {
        return std::make_unique<ZMQ::Endpoint>(*this, config);
    }
};
}  // namespace

namespace dlsm {

Transport::UPtr Transport::create(const std::string_view& options) { return std::make_unique<ZMQ>(options); }
}  // namespace dlsm