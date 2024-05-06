#pragma once

#include <nlohmann/json.hpp>
#include "impl/Logger.hpp"
#include "impl/Transport.hpp"

using namespace std::literals;

namespace dlsm {

class Node {
public:

    using Transport = dlsm::Transport<dlsm::ZMQ>;

    const nlohmann::json config_;
    dlsm::Logger log_;
    Transport transport_;
    Transport::Pub pub_;
    Transport::Sub sub_;
    Transport::Poller poller_;

    Node(nlohmann::json config)
    : config_(std::move(config))
    , log_{config_.value("log","debug:stdout:")}
    , transport_{config_.value("transport","io_threads=1")}
    , pub_{transport_.pub(config_.at("commands").get<std::string>())}
    , sub_{transport_.sub(config_.at("commands").get<std::string>())}
    , poller_{transport_.poller()}
    {
        LOG_INFO("Started config {}", config_.dump(4));
    }
    Node(std::filesystem::path config) : Node{nlohmann::json::parse(std::ifstream{config.string()})} {}

    void send() {
        auto tosend = "1234"s;
        pub_.send(tosend);
    }

    void recv() {
        auto torecv = "000011111111"s;
        sub_.recv(torecv);

        LOG_INFO("Started recv {}", torecv);
    }
};

}  // namespace dlsm
