#include <list>
#include <vector>

#include "impl/Transport.hpp"
#include "unit/Unit.hpp"

using namespace std::literals;

TEST(Transport, DISABLED_IOXConstruction) {
    using Transport = dlsm::Transport<dlsm::IOX>;
    auto impl = Transport("name=iox,inproc=on,pools=64x1000,log=off,monitor=on");

    EXPECT_THAT([] { Transport(""); }, ThrowsMessage<std::invalid_argument>("Missing required key=value for key:name"))
        << "Missing configuration values";

    EXPECT_THAT([] { Transport("name=iox,inproc=on,log=bad"); }, ThrowsMessage<std::out_of_range>("map::at"))
        << "Invalid configuration values";

    EXPECT_THAT([&] { impl.pub("service=bad"); },
                ThrowsMessage<std::invalid_argument>(
                    "Unexpected IOX option value: service=bad expected: service/instance/event"));

    EXPECT_NO_THROW(impl.pub("service=test/i/test"));
    EXPECT_NO_THROW(impl.sub("service=test/i/test"));

    EXPECT_NO_THROW(impl.pub("service=test/i/test,history=10,oncreate=off,onfull=wait,node=Pub"));
    EXPECT_NO_THROW(impl.sub("service=test/i/test,history=10,oncreate=off,onfull=wait,node=Sub,capacity=5"));
}

TEST(Transport, DISABLED_IOXPubSub) {
    auto impl = dlsm::Transport<dlsm::IOX>("name=iox,inproc=on,pools=64x1000,log=off");

    auto pub = impl.pub("service=test/i/test");
    auto sub = impl.sub("service=test/i/test");

    {
        auto s = pub.loan<int>();
        ASSERT_TRUE(s);
        s.value() = 42;
        s.publish();
    }
    {
        auto s = sub.take<int>();
        ASSERT_TRUE(s);
        EXPECT_EQ(s.value(), 42);
    }

    struct Payload {
        int data = 42;

        bool operator==(const Payload& that) const { return this->data == that.data; }
    };

    const auto SendRecv = [&](const auto& tosend, auto torecv) {
        EXPECT_TRUE(pub.send(tosend));
        EXPECT_TRUE(sub.recv(torecv));
        EXPECT_EQ(torecv, tosend);
    };
    SendRecv(1234, 0);
    SendRecv("1234", "1234"s);
    SendRecv(Payload{42}, Payload{0});
    SendRecv("std::string"s, "std__string"s);
    SendRecv(std::vector<Payload>{{3}, {2}, {1}}, std::vector<Payload>{{1}, {2}, {3}});
    SendRecv(std::list<int>{1, 2, 3, 4}, std::list<int>{5, 6, 7, 8});
}

TEST(Transport, ZMQConstruction) {
    using Transport = dlsm::Transport<dlsm::ZMQ>;
    auto impl = Transport("io_threads=1");

    EXPECT_THAT([] { Transport("io_threads=A"); },
                ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:io_threads=A with:stoi"))
        << "Invalid configuration values";

    EXPECT_THAT([] { Transport("thread=1"); },
                ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:thread=1 with:map::at"))
        << "Invalid configuration values";

    EXPECT_THAT([] { Transport("max_sockets=2,cpu_add=1,cpu_rem=1,ipv6=1,sched_policy=-1"); },
                ThrowsMessage<std::invalid_argument>(
                    "Unexpected ZMQ Context option:sched_policy=-1 with: Error:Invalid argument"));

    EXPECT_THAT(
        [] { Transport("priority=-1"); },
        ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:priority=-1 with: Error:Invalid argument"));

    EXPECT_THAT([&] { impl.pub("endpoint=tcp://bad"); },
                ThrowsMessage<std::runtime_error>("Bind to 'tcp://bad' Error:Invalid argument"));

    EXPECT_THAT([&] { impl.sub("endpoint=tcp://bad"); },
                ThrowsMessage<std::runtime_error>("Connect to 'tcp://bad' Error:Invalid argument"));

    EXPECT_THAT(
        [&] { impl.pub("endpoint=inproc://inproc-pub-sub,ipv6=25"); },
        ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Endpoint option:ipv6=25 with: Error:Invalid argument"));
}

TEST(Transport, ZMQPubSub) {
    auto impl = dlsm::Transport<dlsm::ZMQ>("io_threads=1");

    for (const auto& e : {"inproc://inproc-pub-sub"s, "ipc://@ipc-pub-sub"s, "tcp://127.0.0.1:5555"s}) {
        auto pub = impl.pub("endpoint=" + e +
                            ",delay_ms=10"
                            ",send_buf_size=256,send_hwm_msgs=10,send_timeout_ms=10");
        auto sub = impl.sub("endpoint=" + e +
                            ",delay_ms=50,connect_timeout_ms=20"
                            ",recv_buf_size=256,recv_hwm_msgs=10,recv_timeout_ms=10");

        {
            auto s = pub.loan<int>();
            ASSERT_TRUE(s);
            s.value() = 42;
            s.publish();
        }
        {
            auto s = sub.take<int>();
            ASSERT_TRUE(s);
            EXPECT_EQ(s.value(), 42);
        }

        struct Payload {
            int data = 42;

            bool operator==(const Payload& that) const { return this->data == that.data; }
        };

        const auto SendRecv = [&](const auto& tosend, auto torecv) {
            EXPECT_TRUE(pub.send(tosend));
            EXPECT_TRUE(sub.recv(torecv));
            EXPECT_EQ(torecv, tosend);
        };
        SendRecv(1234, 0);
        SendRecv("1234", "1234"s);
        SendRecv(Payload{42}, Payload{0});
        SendRecv("std::string"s, "std__string"s);
        SendRecv(std::vector<Payload>{{3}, {2}, {1}}, std::vector<Payload>{{1}, {2}, {3}});
        SendRecv(std::list<int>{1, 2, 3, 4}, std::list<int>{5, 6, 7, 8});

        {
            auto abc = impl.sub("type=sub,endpoint=" + e + ",subscribe=abc+bbb");

            std::string expected = "abc ABCabca";
            std::string received(expected.size(), ' ');

            EXPECT_TRUE(pub.send("not abc"));
            EXPECT_TRUE(pub.send(expected));
            EXPECT_TRUE(abc.recv(received));
            EXPECT_EQ(received, expected);
        }
    }
}
