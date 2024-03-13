#include <latch>
#include <thread>

#include "impl/Thread.hpp"
#include "unit/Unit.hpp"

TEST(Thread, Yield) {
    EXPECT_NO_THROW({ dlsm::Thread::pause(); });
    EXPECT_NO_THROW({ dlsm::Thread::BusyPause::pause(); });
    EXPECT_NO_THROW({ dlsm::Thread::NanoSleep::pause(); });
}

TEST(Thread, Name) {
    EXPECT_THAT([] { dlsm::Thread::name("16-bytes-is-max-size"); },
                ThrowsMessage<std::system_error>("pthread_setname_np(): Numerical result out of range"));

    EXPECT_NO_THROW({
        std::latch enter{1};
        std::latch exit{1};

        std::jthread t([&] {
            enter.count_down();
            exit.wait();
        });

        enter.wait();
        const std::string expected = "new-thread-name";
        dlsm::Thread::name(expected, t.native_handle());
        auto name = dlsm::Thread::name(t.native_handle());
        EXPECT_EQ(name, expected);
        exit.count_down();
    });
}

TEST(Thread, Affinity) {
    EXPECT_THAT([] { dlsm::Thread::affinity(256); },
                ThrowsMessage<std::system_error>("pthread_setaffinity_np(): Invalid argument"));

    EXPECT_NO_THROW({
        std::latch enter{1};
        std::latch exit{1};

        std::jthread t([&] {
            enter.count_down();
            exit.wait();
        });

        enter.wait();
        EXPECT_EQ(dlsm::Thread::getaffinity(t.native_handle()), 0);
        dlsm::Thread::affinity(1ULL, t.native_handle());
        EXPECT_EQ(dlsm::Thread::getaffinity(t.native_handle()), 1ULL);
        dlsm::Thread::affinity(dlsm::Thread::AllCPU, t.native_handle());
        exit.count_down();
    });
}
