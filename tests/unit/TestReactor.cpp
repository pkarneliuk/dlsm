#include "impl/Reactor.hpp"
#include "unit/Unit.hpp"

#include <list>
#include <thread>

using namespace std::literals;
using dlsm::Reactor;

TEST(Reactor, Construction) {
    auto impl = Reactor("io_threads=1");

    EXPECT_THAT([] { Reactor("io_threads=A"); },
                ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:io_threads=A with:stoi"))
        << "Invalid configuration values";

    EXPECT_THAT([] { Reactor("thread=1"); },
                ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:thread=1 with:map::at"))
        << "Invalid configuration values";

    EXPECT_THAT([] { Reactor("max_sockets=2,cpu_add=1,cpu_rem=1,ipv6=1,sched_policy=-1"); },
                ThrowsMessage<std::invalid_argument>(
                    "Unexpected ZMQ Context option:sched_policy=-1 with: Error:Invalid argument"));

    EXPECT_THAT(
        [] { Reactor("priority=-1"); },
        ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:priority=-1 with: Error:Invalid argument"));
}

TEST(Reactor, Timers) {
    using Timers = dlsm::Reactor::Timers;
    auto impl = Timers();

    struct TestHandler : public Timers::ITimer::Handler {
        bool handled = false;
        void handler(Timers::ITimer& timer) override {
            handled = true;
            timer.cancel();
        }
    } handler;

    auto timer = impl.timer(handler, 10ms);

    std::this_thread::sleep_for(impl.timeout());

    EXPECT_FALSE(handler.handled);
    EXPECT_TRUE(timer->active());
    impl.execute();
    EXPECT_TRUE(handler.handled);
    EXPECT_FALSE(timer->active());
}

TEST(Reactor, Endpoint) {
    auto r = Reactor("");
    auto pub = r.endpoint("type=pub,endpoint=tcp://127.0.0.1:5555");
    auto sub = r.endpoint("type=sub,endpoint=tcp://127.0.0.1:5555");

    auto send = Reactor::Message{pub};
    auto recv = Reactor::Message{sub};

    struct Payload {
        int data = 42;
        bool operator==(const Payload& that) const { return this->data == that.data; }
    };

    const auto SendRecv = [&](const auto& tosend, auto torecv) {
        EXPECT_TRUE(send.send(tosend));
        EXPECT_TRUE(recv.recv(torecv));
        EXPECT_EQ(torecv, tosend);
    };
    SendRecv(1234, 0);
    SendRecv(Payload{42}, Payload{0});
    SendRecv("std::string"s, "empty"s);
    SendRecv(std::vector<Payload>{{3}, {2}, {1}}, std::vector<Payload>{{4}});
    SendRecv(std::list<int>{1, 2, 3, 4}, std::list<int>{5, 6, 7, 8});
}

TEST(Reactor, Once) {
    auto r = Reactor("");

    struct TestHandler : public Reactor::Handler, public Reactor::Timers::ITimer::Handler {

        Reactor::Endpoint::Ptr pub;
        Reactor::Endpoint::Ptr sub;
        Reactor::Timers::ITimer::Ptr timer;
        TestHandler(Reactor& r)
        : pub{r.endpoint("type=pub,endpoint=tcp://127.0.0.1:5555")}
        , sub{r.endpoint("type=sub,endpoint=tcp://127.0.0.1:5555")} {

            timer = r.timers_.timer(*this, 10ms);
            r.add(*this);
        }


        void* socket() override {
            return pub->socket();
        }
        void onIn(Reactor& r) override {

        }
        void onOut(Reactor& r) override {
            std::cout << "onOut" << std::endl;
        }
        void onError(Reactor& r) override {

        }

        bool handled = false;
        void handler(Reactor::Timers::ITimer& timer) override {
            handled = true;
            timer.cancel();
        }
    } handler(r);


    std::this_thread::sleep_for(r.timers_.timeout());
    EXPECT_FALSE(handler.handled);
    EXPECT_TRUE(handler.timer->active());
    r.once(-1ms);
    EXPECT_TRUE(handler.handled);
    EXPECT_FALSE(handler.timer->active());

}
