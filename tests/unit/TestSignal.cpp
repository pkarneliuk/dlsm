#include <barrier>
#include <thread>

#include "impl/Signal.hpp"
#include "unit/Unit.hpp"

namespace {
volatile int handler_called = 0;  // NOLINT
}

TEST(Signal, Type) {
    EXPECT_EQ(dlsm::Signal::str(dlsm::Signal::NONE), "Unknown signal 0");
    EXPECT_EQ(dlsm::Signal::str(dlsm::Signal::BUS), "Bus error");

    EXPECT_NO_THROW(dlsm::Signal::send(dlsm::Signal::NONE););

    EXPECT_THAT([] { dlsm::Signal::send(static_cast<dlsm::Signal::Type>(-1)); },  // NOLINT
                ThrowsMessage<std::runtime_error>(StartsWith("Signal::send(-1) failed: Invalid argument")));
}

TEST(Signal, Guard) {
    EXPECT_THAT([] { dlsm::Signal::Guard(dlsm::Signal::NONE, nullptr); },
                ThrowsMessage<std::runtime_error>(StartsWith("sigaction() failed to set callback: Unknown signal 0")));

    EXPECT_NO_THROW(dlsm::Signal::Guard(dlsm::Signal::BUS, nullptr));

    dlsm::Signal::Handler bus = [](int signal, siginfo_t*, void*) { handler_called = signal; };
    dlsm::Signal::Guard A(dlsm::Signal::BUS, bus);
    EXPECT_TRUE(A.restoring());
    dlsm::Signal::Guard B{std::move(A)};
    EXPECT_TRUE(B.restoring());

    EXPECT_EQ(handler_called, 0);
    dlsm::Signal::send(dlsm::Signal::BUS);
    EXPECT_EQ(handler_called, dlsm::Signal::BUS);
}

TEST(Signal, Termintaion) {
    {
        dlsm::Signal::Termination watcher;
        EXPECT_FALSE(watcher);
        dlsm::Signal::send(dlsm::Signal::TERM);
        EXPECT_TRUE(watcher);
    }

    {
        std::barrier sync(2);
        dlsm::Signal::Termination watcher;
        std::jthread t([&] {
            sync.arrive_and_wait();
            dlsm::Signal::send(dlsm::Signal::TERM);
        });

        EXPECT_FALSE(watcher);
        sync.arrive_and_wait();
        watcher.wait();
        EXPECT_TRUE(watcher);
        EXPECT_EQ(watcher.value(), dlsm::Signal::TERM);
        // EXPECT_EQ(dlsm::Signal::str(watcher.info()), "");

        EXPECT_THAT(dlsm::Signal::str(watcher.info()), MatchesRegex(R"(^PID\:\d+ signal\:\d+ addr:0x[0-9a-fA-F]+$)"));
    }

    EXPECT_EQ(dlsm::Signal::Termination::value(), dlsm::Signal::NONE);
}
