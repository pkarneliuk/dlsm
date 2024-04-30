#include <barrier>
#include <format>
#include <thread>

#include "impl/DisruptorGraph.hpp"
#include "unit/Unit.hpp"

using namespace dlsm::Disruptor::Graph;

namespace TestDisruptorGraph {
struct alignas(64) Event {
    std::uint64_t number{0xFF};  // #event
};
}  // namespace TestDisruptorGraph

using TestDisruptorGraph::Event;

namespace {

using Strs = std::vector<std::string>;

struct WaitProducer {
    static void run(auto ring, auto& barrier, std::size_t count, const std::size_t init, const std::size_t step) {
        std::uint64_t number = init;

        for (std::size_t i = 0; i < count; ++i) {
            const auto seq = barrier.claim() - 1;

            auto& event = ring[seq];
            event.number = number;
            // std::cout << std::format("Pub#{}: {} seq: {}\n", init, number, seq);
            number += step;

            EXPECT_GE(seq, i) << "claimed sequence number must == #event";
            barrier.publish(seq);
        }

        EXPECT_EQ(number, count * step + init);
    }
};

struct WaitConsumer {
    static void run(auto ring, auto& barrier, std::size_t count, const std::size_t producers = 1) {
        dlsm::Disruptor::Sequence::Value next = barrier.last() + 1;

        std::vector<std::size_t> expected(producers, 0);
        for (std::size_t i = 0; i < producers; ++i) expected[i] = i + 1;

        for (std::size_t i = 0; i < count * producers;) {
            auto available = barrier.consume(next);
            EXPECT_GE(available, next) << "at least next sequence number is available";
            for (; next <= available;) {
                const auto& event = ring[next];

                auto step = event.number % producers;
                if (step == 0) step = producers;
                auto index = step - 1;

                // std::cout << std::format("Sub#{}: {} seq: {}\n", index+1, event.number, next);
                auto& exp = expected[index];

                EXPECT_EQ(event.number, exp)
                    << "#event " << event.number << " != expected: " << exp << " for: " << index + 1;
                exp = event.number + producers;

                barrier.release(next);
                ++next;
                ++i;
            }
        }

        for (std::size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(expected[i], (i + 1) + count * producers);
        }
    }
};

struct NoWaitProducer {
    static void run(auto ring, auto& barrier, std::size_t count, const std::size_t init, const std::size_t step) {
        std::uint64_t number = init;
        for (std::size_t i = 0; i < count;) {
            const auto r = barrier.tryClaim(1);
            if (r == dlsm::Disruptor::Sequence::Initial) continue;
            const auto seq = r - 1;

            auto& event = ring[seq];
            event.number = number;
            number += step;

            EXPECT_GE(seq, i) << "claimed sequence number must == #event";
            barrier.publish(seq);
            ++i;
        }
        EXPECT_EQ(number, count * step + init);
    }
};

struct NoWaitConsumer {
    static void run(auto ring, auto& barrier, std::size_t count, const std::size_t producers = 1) {
        dlsm::Disruptor::Sequence::Value next = barrier.last() + 1;

        std::vector<std::size_t> expected(producers, 0);
        for (std::size_t i = 0; i < producers; ++i) expected[i] = i + 1;

        for (std::size_t i = 0; i < count * producers;) {
            const auto available = barrier.consumable(next);
            if (available == dlsm::Disruptor::Sequence::Initial) continue;
            EXPECT_GE(available, next) << "at least next sequence number is available";
            for (; next <= available;) {
                const auto& event = ring[next];

                auto step = event.number % producers;
                if (step == 0) step = producers;
                auto index = step - 1;

                // std::cout << std::format("Sub#{}: {} seq: {}\n", index+1, event.number, next);
                auto& exp = expected[index];

                EXPECT_EQ(event.number, exp)
                    << "#event " << event.number << " != expected: " << exp << " for: " << index + 1;
                exp = event.number + producers;

                barrier.release(next);
                ++next;
                ++i;
            }
        }

        for (std::size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(expected[i], (i + 1) + count * producers);
        }
    }
};

template <typename Producer = WaitProducer, typename Consumer = WaitConsumer>
struct InExternMemory {
    static constexpr std::size_t ToSend = 5000;
    Layout layout;
    std::vector<std::byte> space;

    InExternMemory(Type type, Wait wait, std::size_t pubs = 3, std::size_t subs = 3) {
        auto capacity = Layout::Items::create<Event>(512);
        if (type == Type::SPMC) pubs = 1;
        layout = Layout{{type, wait}, {pubs, subs}, capacity};
        space.resize(layout.size());
    }

    std::jthread Produce(IGraph::Ptr& g, auto& barrier, std::size_t init = 1, std::size_t p = 1) {
        return std::jthread{[&, init, p] { Producer::run(g->ring<Event>(), *barrier, ToSend, init, p); }};
    }
    std::jthread Consume(IGraph::Ptr& g, auto& barrier, std::size_t p = 1) {
        return std::jthread{[&, p] { Consumer::run(g->ring<Event>(), *barrier, ToSend, p); }};
    }

    void Unicast() {  // Unicast: P1 –> C1
        auto graph = IGraph::inproc(layout, space);
        auto P1 = graph->pub("P1");
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("P1"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        auto TP1 = Produce(graph, P1);
        auto TC1 = Consume(graph, C1);
    }
    void Pipeline() {  // Pipeline: P1 –> C1 -> C2 -> C3
        auto graph = IGraph::inproc(layout, space);
        auto P1 = graph->pub("P1");
        auto C1 = graph->sub("C1");
        auto C2 = graph->sub("C2", {{"C1"}});
        auto C3 = graph->sub("C3", {{"C2"}});
        EXPECT_THAT(graph->dependencies("P1"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C3"), Strs{"C2"});
        auto TP1 = Produce(graph, P1);
        auto TC1 = Consume(graph, C1);
        auto TC2 = Consume(graph, C2);
        auto TC3 = Consume(graph, C3);
    }
    void Sequencer() {  // Sequencer: P1,P2,P3 –> C1,C2 -> C3

        if (layout.graph_.type_ != Type::MPMC) return;  // Only MPMC supports multiple Producers

        auto graph = IGraph::inproc(layout, space);
        auto P1 = graph->pub("P1");
        auto P2 = graph->pub("P2");
        auto P3 = graph->pub("P3");
        EXPECT_TRUE(graph->dependencies("P1").empty());
        EXPECT_TRUE(graph->dependencies("P2").empty());
        EXPECT_TRUE(graph->dependencies("P3").empty());
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("P1"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("P2"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("P3"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        auto C2 = graph->sub("C2");
        EXPECT_THAT(graph->dependencies("P1"), ContainerEq(Strs{"C1", "C2"}));
        EXPECT_THAT(graph->dependencies("P2"), ContainerEq(Strs{"C1", "C2"}));
        EXPECT_THAT(graph->dependencies("P3"), ContainerEq(Strs{"C1", "C2"}));
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"Master"});
        auto C3 = graph->sub("C3", {{"C1", "C2"}});
        EXPECT_THAT(graph->dependencies("P1"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("P2"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("P3"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C3"), ContainerEq(Strs{"C1", "C2"}));
        std::size_t producers = 3;
        auto TP1 = Produce(graph, P1, 1, producers);
        auto TP2 = Produce(graph, P2, 2, producers);
        auto TP3 = Produce(graph, P3, 3, producers);
        auto TC1 = Consume(graph, C1, producers);
        auto TC2 = Consume(graph, C2, producers);
        auto TC3 = Consume(graph, C3, producers);
    }
    void Multicast() {  // Multicast: P1 –> C1,C2,C3
        auto graph = IGraph::inproc(layout, space);
        auto P1 = graph->pub("P1");
        auto C1 = graph->sub("C1");
        auto C2 = graph->sub("C2");
        auto C3 = graph->sub("C3");
        EXPECT_THAT(graph->dependencies("P1"), ContainerEq(Strs{"C1", "C2", "C3"}));
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C3"), Strs{"Master"});
        auto TP1 = Produce(graph, P1);
        auto TC1 = Consume(graph, C1);
        auto TC2 = Consume(graph, C2);
        auto TC3 = Consume(graph, C3);
    }
    void Diamond() {  // Diamond: P1 –> C1,C2 -> C3
        auto graph = IGraph::inproc(layout, space);
        auto P1 = graph->pub("P1");
        auto C1 = graph->sub("C1");
        auto C2 = graph->sub("C2");
        auto C3 = graph->sub("C3", {{"C1", "C2"}});
        EXPECT_THAT(graph->dependencies("P1"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C3"), ContainerEq(Strs{"C1", "C2"}));
        auto TP1 = Produce(graph, P1);
        auto TC1 = Consume(graph, C1);
        auto TC2 = Consume(graph, C2);
        auto TC3 = Consume(graph, C3);
    }
    void Topologies() {
        Unicast();
        Pipeline();
        Sequencer();
        Multicast();
        Diamond();
    }
};

template <typename Producer = WaitProducer, typename Consumer = WaitConsumer>
struct InSharedMemory {
    static constexpr std::size_t ToSend = 1000;
    const Layout layout;

    InSharedMemory(const Layout& in) : layout{in} {}

    std::string opts() const { return std::format("name=DisruptorGraph.InSharedMemory{},lock=on", ::getpid()); }

    std::jthread pub(std::barrier<>& sync, std::string name, std::size_t init = 1, std::size_t publishers = 1) const {
        return std::jthread([this, name, init, publishers, &sync] {
            dlsm::Thread::name(name);
            std::string create = (init == 1) ? ",purge=on,create=on" : ",open=500x1";
            auto graph = IGraph::shared(layout, opts() + create);
            auto pub = graph->pub(name);
            sync.arrive_and_wait();
            Producer::run(graph->template ring<Event>(), *pub, ToSend, init, publishers);
            if (publishers == 1) {
                EXPECT_EQ(pub->next(), ToSend);
            }
            // std::cout << std::format("{}:{}\n", name, graph->description());
        });
    };

    std::jthread sub(std::barrier<>& sync, std::string name, const std::vector<std::string>& deps = {},
                     std::size_t publishers = 1) const {
        return std::jthread([this, &sync, name, deps, publishers] {
            dlsm::Thread::name(name);
            std::vector<std::string_view> svdeps;
            svdeps.reserve(deps.size());
            for (auto& s : deps) svdeps.emplace_back(std::string_view{s});
            auto graph = IGraph::shared(layout, opts() + ",open=500x1");
            auto sub = graph->sub(name, svdeps);
            sync.arrive_and_wait();
            Consumer::run(graph->template ring<Event>(), *sub, ToSend, publishers);
            EXPECT_EQ(sub->last(), (publishers * ToSend) - 1);
            // std::cout << std::format("{}:{}\n", name, graph->description());
        });
    };

    void Unicast() {  // Unicast: P1 –> C1
        std::barrier sync(2);
        auto P1 = pub(sync, "P1");
        auto C1 = sub(sync, "C1");
    }
    void Pipeline() {  // Pipeline: P1 –> C1 -> C2 -> C3
        std::barrier sync(4);
        auto P1 = pub(sync, "P1");
        auto C1 = sub(sync, "C1");
        auto C2 = sub(sync, "C2", Strs{"C1"});
        auto C3 = sub(sync, "C3", Strs{"C2"});
    }
    void Sequencer() {  // Sequencer: P1,P2,P3 –> C1,C2 -> C3
        std::barrier sync(6);
        std::size_t publishers = 3;
        auto P1 = pub(sync, "P1", 1, publishers);
        auto P2 = pub(sync, "P2", 2, publishers);
        auto P3 = pub(sync, "P3", 3, publishers);
        auto C1 = sub(sync, "C1", Strs{}, publishers);
        auto C2 = sub(sync, "C2", Strs{}, publishers);
        auto C3 = sub(sync, "C3", Strs{"C1", "C2"}, publishers);
    }
    void Multicast() {  // Multicast: P1 –> C1,C2,C3
        std::barrier sync(4);
        auto P1 = pub(sync, "P1");
        auto C1 = sub(sync, "C1");
        auto C2 = sub(sync, "C2");
        auto C3 = sub(sync, "C3");
    }
    void Diamond() {  // Diamond: P1 –> C1,C2 -> C3
        std::barrier sync(4);
        auto P1 = pub(sync, "P1");
        auto C1 = sub(sync, "C1");
        auto C2 = sub(sync, "C2");
        auto C3 = sub(sync, "C3", Strs{"C1", "C2"});
    }
    void Topologies() {
        Unicast();
        Pipeline();
        if (layout.graph_.type_ == Type::MPMC) Sequencer();  // SPMC does not support multiple Producers
        Multicast();
        Diamond();
    }
};

}  // namespace

TEST(DisruptorGraph, Layout) {
    Layout layout;
    EXPECT_EQ(layout.graph_.wait_, Wait::Block);
    const auto empty = layout.size();
    EXPECT_GE(empty, 128);

    layout = Layout({}, {}, Layout::Items::create<double>(0, "double"));
    EXPECT_EQ(layout.size(), empty);
    EXPECT_EQ(layout.items_.type(), "double");

    EXPECT_THAT(
        [&] {
            layout.check(Layout::Graph{Type::MPMC, Wait::Yield});
        },
        ThrowsMessage<std::runtime_error>("Layout::Graph missmatch: type:SPSC=MPMC wait:Block=Yield"));
    EXPECT_THAT(
        [&] {
            layout.check(Layout::Slots{1, 4});
        },
        ThrowsMessage<std::runtime_error>("Layout::Slots missmatch: maxPub:0=1 maxSub:0=4"));
    EXPECT_THAT([&] { layout.check(Layout::Items::create<float>(0, "float")); },
                ThrowsMessage<std::runtime_error>("Layout::Items missmatch: size:8=4 align:8=4 type:double=float"));
}

TEST(DisruptorGraph, RefCounting) {
    auto i = Layout::Items::create<Event>(2);
    {
        auto graph = IGraph::create(Type::SPMC, Wait::Spins, i);
        EXPECT_EQ(graph.use_count(), 1);
        auto C1 = graph->sub("C1");
        EXPECT_EQ(graph.use_count(), 2);
        EXPECT_EQ(C1.use_count(), 1);
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        auto P1 = graph->pub("P1");
        EXPECT_EQ(graph.use_count(), 3);
        EXPECT_EQ(C1.use_count(), 1);
        EXPECT_EQ(P1.use_count(), 1);
        C1.reset();
        EXPECT_EQ(graph.use_count(), 2);
        P1.reset();
        EXPECT_EQ(graph.use_count(), 1);
        EXPECT_TRUE(graph.unique());
    }
}

TEST(DisruptorGraph, Description) {
    auto i = Layout::Items::create<Event>(2);

    auto graph = IGraph::create(Type::MPMC, Wait::Spins, i);
    auto P1 = graph->pub("P1");
    P1->publish(P1->claim());
    std::string placeholder = graph->description();
    EXPECT_FALSE(placeholder.empty());
}

TEST(DisruptorGraph, Linking) {
    auto i = Layout::Items::create<Event>(2);
    {
        // Pipeline: P1 -> C1
        auto graph = IGraph::create(Type::SPMC, Wait::Spins, i);
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
    }

    {
        // Pipeline: P1 -> C1 -> C2
        auto graph = IGraph::create(Type::SPMC, Wait::Spins, i);
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        auto C2 = graph->sub("C2", {{"C1"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C2"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
    }
    {
        // Pipeline: P1 -> C1 -> C2
        auto graph = IGraph::create(Type::SPMC, Wait::Spins, i);
        auto C2 = graph->sub("C2", {{"C1"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C2"}) << graph->description();
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"}) << graph->description();
        EXPECT_THAT(graph->dependencies("C1"), Strs{});
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C2"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
    }

    {
        // Pipeline: P1 -> C1 -> C2 -> C3
        auto graph = IGraph::create(Type::SPMC, Wait::Spins, i);
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        auto C2 = graph->sub("C2", {{"C1"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C2"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
        auto C3 = graph->sub("C3", {{"C2"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C3"), Strs{"C2"});
    }
    {
        // Pipeline: P1 -> C1 -> C2 -> C3
        auto graph = IGraph::create(Type::SPMC, Wait::Spins, i);
        auto C3 = graph->sub("C3", {{"C2"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"}) << graph->description();
        EXPECT_THAT(graph->dependencies("C3"), Strs{"C2"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{});
        auto C2 = graph->sub("C2", {{"C1"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"}) << graph->description();
        EXPECT_THAT(graph->dependencies("C1"), Strs{});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C3"), Strs{"C2"});
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"}) << graph->description();
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C3"), Strs{"C2"});
    }
    {
        // Pipeline: P1 -> C1 -> C2 -> C3
        auto graph = IGraph::create(Type::SPMC, Wait::Spins, i);
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        auto C3 = graph->sub("C3", {{"C2"}});
        EXPECT_THAT(graph->dependencies("Master"), (Strs{"C1", "C3"}));
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{});
        EXPECT_THAT(graph->dependencies("C3"), Strs{"C2"});
        auto C2 = graph->sub("C2", {{"C1"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C3"), Strs{"C2"});
    }
    {
        // Pipeline: P1 -> C1 -> C2 -> C3
        auto graph = IGraph::create(Type::SPMC, Wait::Spins, i);
        auto C2 = graph->sub("C2", {{"C1"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C2"}) << graph->description();
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"}) << graph->description();
        EXPECT_THAT(graph->dependencies("C1"), Strs{});
        auto C3 = graph->sub("C3", {{"C2"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C3"), Strs{"C2"});
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C3"), Strs{"C2"});
    }

    {
        // Sequencer: P1,P2,P3 –> C1,C2 -> C3
        auto graph = IGraph::create(Type::MPMC, Wait::Spins, i);
        auto P1 = graph->pub("P1");
        auto P2 = graph->pub("P2");
        auto P3 = graph->pub("P3");
        EXPECT_TRUE(graph->dependencies("P1").empty());
        EXPECT_TRUE(graph->dependencies("P2").empty());
        EXPECT_TRUE(graph->dependencies("P3").empty());
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("P1"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("P2"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("P3"), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        auto C2 = graph->sub("C2");
        EXPECT_THAT(graph->dependencies("P1"), ContainerEq(Strs{"C1", "C2"}));
        EXPECT_THAT(graph->dependencies("P2"), ContainerEq(Strs{"C1", "C2"}));
        EXPECT_THAT(graph->dependencies("P3"), ContainerEq(Strs{"C1", "C2"}));
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"Master"});
        auto C3 = graph->sub("C3", {{"C1", "C2"}});
        EXPECT_THAT(graph->dependencies("P1"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("P2"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("P3"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C3"), ContainerEq(Strs{"C1", "C2"}));
    }

    {
        // Diamond: P1 –> C1,C2 -> C3
        auto graph = IGraph::create(Type::MPMC, Wait::Spins, i);
        auto P1 = graph->pub("P1");
        EXPECT_TRUE(graph->dependencies().empty());
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies(), Strs{"C1"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        auto C2 = graph->sub("C2");
        EXPECT_THAT(graph->dependencies(), ContainerEq(Strs{"C1", "C2"}));
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"Master"});
        auto C3 = graph->sub("C3", {{"C1", "C2"}});
        EXPECT_THAT(graph->dependencies(), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C3"), ContainerEq(Strs{"C1", "C2"}));
    }
    {
        // Diamond: P1 –> C1,C2 -> C3
        auto graph = IGraph::create(Type::MPMC, Wait::Spins, i);
        auto P1 = graph->pub("P1");
        EXPECT_TRUE(graph->dependencies().empty());
        auto C3 = graph->sub("C3", {{"C1", "C2"}});
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{});
        EXPECT_THAT(graph->dependencies("C2"), Strs{});
        EXPECT_THAT(graph->dependencies("C3"), ContainerEq(Strs{"C1", "C2"}));
        auto C1 = graph->sub("C1");
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{});
        EXPECT_THAT(graph->dependencies("C3"), ContainerEq(Strs{"C1", "C2"}));
        auto C2 = graph->sub("C2");
        EXPECT_THAT(graph->dependencies("Master"), Strs{"C3"});
        EXPECT_THAT(graph->dependencies("C1"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C2"), Strs{"Master"});
        EXPECT_THAT(graph->dependencies("C3"), ContainerEq(Strs{"C1", "C2"}));
    }
}

TEST(DisruptorGraph, NoWait) {
    InExternMemory<NoWaitProducer, NoWaitConsumer>(Type::SPMC, Wait::Spins).Topologies();
    InExternMemory<NoWaitProducer, NoWaitConsumer>(Type::MPMC, Wait::Spins).Topologies();

    InExternMemory<NoWaitProducer, WaitConsumer>(Type::SPMC, Wait::Spins).Topologies();
    InExternMemory<NoWaitProducer, WaitConsumer>(Type::MPMC, Wait::Spins).Topologies();

    InExternMemory<WaitProducer, NoWaitConsumer>(Type::SPMC, Wait::Spins).Topologies();
    InExternMemory<WaitProducer, NoWaitConsumer>(Type::MPMC, Wait::Spins).Topologies();
}

TEST(DisruptorGraph, InExternMemory) {
    InExternMemory(Type::SPMC, Wait::Spins).Topologies();
    InExternMemory(Type::MPMC, Wait::Spins).Topologies();

    InExternMemory(Type::SPMC, Wait::Yield).Topologies();
    InExternMemory(Type::MPMC, Wait::Yield).Topologies();

    InExternMemory(Type::SPMC, Wait::Block).Topologies();
    InExternMemory(Type::MPMC, Wait::Block).Topologies();

    InExternMemory(Type::SPMC, Wait::Share).Topologies();
    InExternMemory(Type::MPMC, Wait::Share).Topologies();

    {
        auto capacity = Layout::Items{512};
        auto layout = Layout{{Type::MPMC, Wait::Block}, {1, 1}, capacity};
        std::vector<std::byte> lowspace{42};

        EXPECT_THAT([&] { IGraph::inproc(layout, lowspace); },
                    ThrowsMessage<std::invalid_argument>(
                        MatchesRegex(R"(IGraph::Layout requires \d+ bytes, only \d+ provided)")));
    }
}

TEST(DisruptorGraph, InSharedMemory) {
    const auto capacity = Layout::Items::create<Event>(64);
    {
        auto layout = Layout{{Type::SPMC, Wait::Spins}, {1, 3}, capacity};
        InSharedMemory{layout}.Topologies();
    }
    {
        auto layout = Layout{{Type::MPMC, Wait::Spins}, {3, 3}, capacity};
        InSharedMemory{layout}.Topologies();
    }
    {
        auto layout = Layout{{Type::SPMC, Wait::Share}, {1, 3}, capacity};
        InSharedMemory{layout}.Topologies();
    }
    {
        auto layout = Layout{{Type::MPMC, Wait::Share}, {3, 3}, capacity};
        InSharedMemory{layout}.Topologies();
    }

    {
        auto layout = Layout{{Type::SPMC, Wait::Block}, {1, 3}, capacity};
        EXPECT_THAT([&] { IGraph::shared(layout, "opts"); },
                    ThrowsMessage<std::invalid_argument>("Wait::Block is not allowed for Layout in Shared Memory"));
    }
    {
        auto layout = Layout{{Type::SPMC, Wait::Yield}, {2, 3}, capacity};
        EXPECT_THAT([&] { IGraph::shared(layout, "name=DisruptorGraph.BadMaxSubs,create=on"); },
                    ThrowsMessage<std::invalid_argument>("Type::SPMC supports only one producer, current limit:2"));
    }
}
