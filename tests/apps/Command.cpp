#include <chrono>
#include <format>
#include <fstream>
#include <iostream>

#include <vector>
#include <string_view>

#include "impl/Node.hpp"
#include "impl/Signal.hpp"

using namespace std::literals;

struct Sends : public dlsm::Node, public dlsm::Node::Transport::Poller::Timer::Handler {
    using Node::Node;

    Transport::Poller::Timer::Ptr timer = poller_.timer(*this, std::chrono::milliseconds(1));

    std::size_t count = 0;

    void handler(Transport::Poller::Timer& timer) override {
        count++;
        std::cout << std::format("Timer {}", count) << std::endl;


        send();
        recv();


        if(count >= 5) {
            std::cout << std::format("Cancelled after {}", count) << std::endl;
            timer.cancel();
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/config.json <cmd1 cmd2 ...>\n";
        return -1;
    }

    const auto path = std::filesystem::path{argv[1]};
    const auto cmds = std::vector<std::string>{argv + 2, argv + argc};
    Sends node(path);
  
    for(const auto& cmd : cmds) {
      std::cout << std::format("> {} :\n", cmd);
      node.pub_.send(cmd);
    }

    std::cout << std::format("Command Config {}: ", path.string()) << node.config_ << std::endl;

    dlsm::Signal::Termination watcher;

    while(!watcher && node.timer->active()) {
        node.poller_.once(-1ms);
    }

    return 0;
}
