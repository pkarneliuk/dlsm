#include <barrier>
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
        std::barrier sync(2);

        std::jthread t([&] {
            sync.arrive_and_wait();
            sync.arrive_and_wait();
        });

        sync.arrive_and_wait();
        const std::string expected = "new-thread-name";
        dlsm::Thread::name(expected, t.native_handle());
        auto name = dlsm::Thread::name(t.native_handle());
        EXPECT_EQ(name, expected);
        sync.arrive_and_wait();
    });
}

TEST(Thread, Affinity) {
    EXPECT_GT(dlsm::Thread::AllCPU.count(), 0);
    EXPECT_GT(dlsm::Thread::AllAvailableCPU().count(), 0);
    EXPECT_THAT([] { dlsm::Thread::affinity(dlsm::Thread::Mask({dlsm::Thread::MaskSize - 1})); },
                ThrowsMessage<std::system_error>("pthread_setaffinity_np(): Invalid argument"));

    const auto cpu01 = dlsm::Thread::Mask{{0U, 1U}};
    EXPECT_EQ(cpu01.count(), 2);
    EXPECT_TRUE(cpu01[0] && cpu01[1]);
    EXPECT_EQ(cpu01.to_string(), "01");
    EXPECT_EQ(dlsm::Thread::Mask{}.to_string(), "");

    EXPECT_NO_THROW({
        std::barrier sync(2);

        std::jthread t([&] {
            sync.arrive_and_wait();
            sync.arrive_and_wait();
        });

        sync.arrive_and_wait();
        EXPECT_EQ(dlsm::Thread::affinity(t.native_handle()), dlsm::Thread::AllCPU);
        dlsm::Thread::affinity(cpu01, t.native_handle());
        EXPECT_EQ(dlsm::Thread::affinity(t.native_handle()), cpu01);
        dlsm::Thread::affinity(dlsm::Thread::AllCPU, t.native_handle());
        sync.arrive_and_wait();
    });

    auto cpu58 = dlsm::Thread::Mask{{5U, 8U}};
    EXPECT_EQ(cpu58.to_string(), ".....5..8");
    EXPECT_THAT([&] { cpu58.at(2); }, ThrowsMessage<std::runtime_error>("No available bits at index:2"));
    EXPECT_THAT([&] { cpu58.extract(3); }, ThrowsMessage<std::runtime_error>("No bits for extraction:3"));

    EXPECT_EQ(cpu58.at(1), dlsm::Thread::Mask({8U}));
    EXPECT_EQ(cpu58.extract(2), dlsm::Thread::Mask({5U, 8U}));
    EXPECT_FALSE(cpu58);
}
