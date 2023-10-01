#include <list>
#include <vector>

#include "impl/Transport.hpp"
#include "unit/Unit.hpp"

using namespace std::literals;

TEST(Transport, Construction) {
    auto impl = dlsm::Transport::create("io_threads=1");
    ASSERT_NE(impl, nullptr);

    EXPECT_THAT([] { dlsm::Transport::create("io_threads=A"); },
                ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:io_threads=A with:stoi"))
        << "Invalid configuration values";

    EXPECT_THAT([] { dlsm::Transport::create("thread=1"); },
                ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:thread=1 with:map::at"))
        << "Invalid configuration values";

    EXPECT_THAT([] { dlsm::Transport::create("max_sockets=2,cpu_add=1,cpu_rem=1,ipv6=1,sched_policy=-1"); },
                ThrowsMessage<std::invalid_argument>(
                    "Unexpected ZMQ Context option:sched_policy=-1 with: Error:Invalid argument"));

    EXPECT_THAT(
        [] { dlsm::Transport::create("priority=-1"); },
        ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:priority=-1 with: Error:Invalid argument"));

    EXPECT_THAT([&] { impl->endpoint("type=pub,endpoint=tcp://bad"); },
                ThrowsMessage<dlsm::Transport::Exception>("Bind to 'tcp://bad' Error:Invalid argument"));

    EXPECT_THAT([&] { impl->endpoint("type=sub,endpoint=tcp://bad"); },
                ThrowsMessage<dlsm::Transport::Exception>("Connect to 'tcp://bad' Error:Invalid argument"));

    EXPECT_THAT(
        [&] { impl->endpoint("type=pub,endpoint=inproc://inproc-pub-sub,ipv6=25"); },
        ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Endpoint option:ipv6=25 with: Error:Invalid argument"));

    EXPECT_THAT([&] { impl->endpoint("type=stream,endpoint=inproc://inproc-pub-sub"); },
                ThrowsMessage<std::invalid_argument>("Endpoint type=stream is unsupported"));
}

TEST(Transport, ZMQPubSub) {
    auto impl = dlsm::Transport::create("io_threads=1");
    ASSERT_NE(impl, nullptr);

    for (const auto& e : {"inproc://inproc-pub-sub"s, "ipc://@ipc-pub-sub"s, "tcp://127.0.0.1:5555"s}) {
        auto pub = impl->endpoint("type=pub,endpoint=" + e +
                                  ",delay_ms=10"
                                  ",send_buf_size=256,send_hwm_msgs=10,send_timeout_ms=10");
        auto sub = impl->endpoint("type=sub,endpoint=" + e +
                                  ",delay_ms=50,connect_timeout_ms=20"
                                  ",recv_buf_size=256,recv_hwm_msgs=10,recv_timeout_ms=10");

        struct Payload {
            int data = 42;

            bool operator==(const Payload& that) const { return this->data == that.data; }
        };

        const auto SendRecv = [&](const auto& tosend, auto torecv) {
            EXPECT_TRUE(pub->send(tosend));
            EXPECT_TRUE(sub->recv(torecv));
            EXPECT_EQ(torecv, tosend);
        };
        SendRecv(1234, 0);
        SendRecv("1234", "1234"s);
        SendRecv(Payload{42}, Payload{0});
        SendRecv("std::string"s, "std__string"s);
        SendRecv(std::vector<Payload>{{3}, {2}, {1}}, std::vector<Payload>{{1}, {2}, {3}});
        SendRecv(std::list<int>{1, 2, 3, 4}, std::list<int>{5, 6, 7, 8});

        {
            auto abc = impl->endpoint("type=sub,endpoint=" + e + ",subscribe=abc+bbb");

            std::string expected = "abc ABCabca";
            std::string received(expected.size(), ' ');

            EXPECT_TRUE(pub->send("not abc"));
            EXPECT_TRUE(pub->send(expected));
            EXPECT_TRUE(abc->recv(received));
            EXPECT_EQ(received, expected);
        }
    }
}

TEST(Transport, ZMQXPubXSub) {
    auto impl = dlsm::Transport::create("io_threads=1");
    ASSERT_NE(impl, nullptr);

    for (const auto& e : {"inproc://inproc-pub-sub"s, "ipc://@ipc-pub-sub"s, "tcp://127.0.0.1:5555"s}) {
        auto pub = impl->endpoint("type=xpub,endpoint=" + e +
                                  ",delay_ms=10"
                                  ",send_buf_size=256,send_hwm_msgs=10,send_timeout_ms=10");
        auto sub = impl->endpoint("type=xsub,endpoint=" + e +
                                  ",delay_ms=50,connect_timeout_ms=20,subscribe=abc"
                                  ",recv_buf_size=256,recv_hwm_msgs=10,recv_timeout_ms=10");

        std::string subscribe(64, '\0');
        const auto size = pub->recv(subscribe);
        subscribe.resize(size);
        EXPECT_EQ(subscribe,
                  "\x1"
                  "abc");

        std::string expected = "abc ABCabca";
        std::string received(expected.size(), ' ');

        EXPECT_TRUE(pub->send("not abc"));
        EXPECT_TRUE(pub->send(expected));
        EXPECT_TRUE(sub->recv(received));
        EXPECT_EQ(received, expected);
    }
}
