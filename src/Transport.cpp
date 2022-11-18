#include "impl/Transport.hpp"

#include <map>
#include <memory>
#include <zmqpp/zmqpp.hpp>

#include "impl/Str.hpp"

namespace {
class ZMQ : public dlsm::Transport {
    static_assert((ZMQPP_VERSION_MAJOR == 4) && (ZMQPP_VERSION_MINOR >= 1), "ZeroMQ++ 4.1+ is required");
    zmqpp::context context_;

public:
    ZMQ(const std::string& options) {
        using Opts = zmqpp::context_option;
        static const auto opts = std::map<std::string, Opts>{
            {"io_threads", Opts::io_threads},
            {"max_sockets", Opts::max_sockets},
            {"thread_sched_policy", Opts::thread_sched_policy},
            {"thread_priority", Opts::thread_priority},
            {"ipv6", Opts::ipv6},
        };

        // Parse options and try to set their values to context
        for (const auto& opt : dlsm::Str::ParseOpts(options)) try {
                context_.set(opts.at(opt.first), std::stoi(opt.second));
            } catch (const std::exception& e) {
                throw std::invalid_argument("Unexpected ZMQ Context option:" + opt.first + "=" + opt.second +
                                            " with:" + e.what());
            }
    }

    class Endpoint : public dlsm::Transport::Endpoint {
        zmqpp::socket socket_;
        using Type = zmqpp::socket_type;

        static Type GetType(const dlsm::Str::ParseOpts& opts) {
            static const auto types = std::map<std::string, Type>{
                // clang-format off
                {"pub", Type::pub},
                {"sub", Type::sub},
                {"req", Type::req},
                {"rep", Type::rep},
                {"xpub", Type::xpub},
                {"xsub", Type::xsub},
                {"pair", Type::pair},
                {"pull", Type::pull},
                {"push", Type::push},
                {"stream", Type::stream},
                // clang-format on
            };
            return types.at(opts.at("type"));
        }

    public:
        Endpoint(ZMQ& zmq, const std::string& config) : Endpoint(zmq, dlsm::Str::ParseOpts(config)) {}
        Endpoint(ZMQ& zmq, const dlsm::Str::ParseOpts& opts) : socket_(zmq.context_, GetType(opts)) {
            const auto& endpoint = opts.at("endpoint");
            switch (socket_.type()) {
                case Type::pub:
                    socket_.bind(endpoint);
                    break;
                case Type::sub:
                    socket_.connect(endpoint);
                    break;
                default:
                    throw std::invalid_argument("Endpoint type=" + opts.at("type") + " is unsupported");
            }
        }
    };

    Endpoint::UPtr CreateEndpoint(const std::string& config) override {
        return std::make_unique<ZMQ::Endpoint>(*this, config);
    }
};
}  // namespace

namespace dlsm {

Transport::UPtr Transport::Create(const std::string& options) { return std::make_unique<ZMQ>(options); }
}  // namespace dlsm