#include <chrono>
#include <format>
#include <iostream>

#include "impl/DisruptorGraph.hpp"
#include "impl/Signal.hpp"

using namespace std::literals;
using namespace dlsm::Disruptor::Graph;

namespace {
struct alignas(64) Event {
    std::chrono::nanoseconds timestamp;
    std::uint64_t seqnumber;
};

std::ostream& operator<<(std::ostream& out, const Event& e) {
    auto tp = std::chrono::system_clock::time_point{e.timestamp};
    auto zt = std::chrono::zoned_time{std::chrono::current_zone(), tp};
    out << std::format("seq:{} ts:{:%T%z}", e.seqnumber, zt);
    return out;
}

void produce(const Event& event, auto& pub, auto& items) {
    const auto seq = pub.claim() - 1;
    items[seq] = event;
    items[seq].seqnumber = static_cast<std::uint64_t>(seq);
    // std::cout << pub.name() << " " << items[seq] << '\n';
    pub.publish(seq);
}

void consume(auto& sub, const auto& items) {
    auto next = sub.last() + 1;
    auto last = sub.consume(next);
    for (; next <= last; ++next) {
        auto& event = items[next];
        std::cout << /*sub.name()*/ "sub" << " " << event << '\n';
    }
    sub.release(next - 1);
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3 || (argv[1] != "pub"s && argv[1] != "sub"s && argv[1] != "mon"s)) {
        std::cerr << "Usage: " << argv[0] << " pub|sub|mon /dev/shm/shared <n=1>\n";
        return -1;
    }

    const auto pub = argv[1] == "pub"s;
    const auto sub = argv[1] == "sub"s;
    [[maybe_unused]] const auto mon = argv[1] == "mon"s;

    auto name = std::string{argv[1]};
    auto path = std::string{argv[2]};
    auto msgs = argc < 4 ? 1 : std::stoull(argv[3]);
    auto data = Layout{{Type::MPMC, Wait::Spins}, {.maxPub_ = 4, .maxSub_ = 4}, Layout::Items::create<Event>(16)};
    auto opts = std::format("lock=on,name={}", path);

    if (pub) opts += ",purge=on,create=on";
    if (sub) opts += ",open=1000x10";

    dlsm::Signal::Termination watcher;

    auto graph = IGraph::shared(data, opts);
    auto items = graph->ring<Event>();

    auto nameC1 = name + "C1";
    auto nameC2 = name + "C2";

    auto P1 = graph->pub(name + "P1");
    auto C1 = graph->sub(nameC1);
    auto C2 = graph->sub(nameC2, {{std::string_view{nameC1}}});

    std::cout << std::format("{} {}", name, graph->description());

    for (std::size_t i = 0; i < msgs; ++i) {
        produce(Event{.timestamp = std::chrono::system_clock::now().time_since_epoch(), .seqnumber = i}, *P1, items);
        consume(*C1, items);
        consume(*C2, items);
    }

    std::cout << std::format("{} {}", name, graph->description());

    watcher.wait();

    return 0;
}
