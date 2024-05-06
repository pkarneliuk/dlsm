#include <chrono>
#include <format>
#include <fstream>
#include <iostream>

#include "impl/Node.hpp"
#include "impl/Signal.hpp"

using namespace std::literals;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/config.json\n";
        return -1;
    }

    const auto path = std::filesystem::path{argv[1]};

    auto conf = nlohmann::json::parse(std::ifstream{path});

    dlsm::Node node(conf);

  //  dlsm::Signal::Termination watcher;


    std::cout << std::format("Config {}: ", path.string()) << node.config_ << std::endl;

    node.send();
    node.recv();

   // watcher.wait();

    return 0;
}
