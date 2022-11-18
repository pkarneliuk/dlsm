#include "impl/Transport.hpp"
#include "unit/Unit.hpp"

TEST(Transport, Construction) {
    auto impl = dlsm::Transport::Create("io_threads=1");
    EXPECT_NE(impl, nullptr);

    EXPECT_THAT([] { dlsm::Transport::Create("io_threads=A"); },
                ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:io_threads=A with:stoi"))
        << "Invalid configuration values";

    EXPECT_THAT([] { dlsm::Transport::Create("thread=1"); },
                ThrowsMessage<std::invalid_argument>("Unexpected ZMQ Context option:thread=1 with:map::at"))
        << "Invalid configuration values";

    auto pub = impl->CreateEndpoint("type=pub,endpoint=inproc://somename");
    auto sub = impl->CreateEndpoint("type=sub,endpoint=inproc://somename");

    EXPECT_THAT([&] { impl->CreateEndpoint("type=stream,endpoint=inproc://somename"); },
                ThrowsMessage<std::invalid_argument>("Endpoint type=stream is unsupported"));
}
