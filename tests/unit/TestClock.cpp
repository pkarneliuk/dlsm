#include "impl/Clock.hpp"
#include "unit/Unit.hpp"

using namespace std::literals;

template <dlsm::Clock::Concept Clock>
void TestClock(std::chrono::nanoseconds resolution) {
    const auto ts1 = Clock::timestamp();
    const auto ts2 = Clock::timestamp();
    EXPECT_NE(ts1, 0ns);
    EXPECT_LE(ts1, ts2);

    const auto res = Clock::resolution();
    EXPECT_NE(res, 0ns);
    EXPECT_LE(res, resolution) << typeid(Clock).name() << " min tick resolution";
}

TEST(Clock, Clock) {
    TestClock<dlsm::Clock::System>(1ns);
    TestClock<dlsm::Clock::Steady>(1ns);
    TestClock<dlsm::Clock::HiRes>(1ns);
    TestClock<dlsm::Clock::Realtime>(1ns);
    TestClock<dlsm::Clock::Monotonic>(1ns);
    TestClock<dlsm::Clock::RealCoarse>(5ms);
    TestClock<dlsm::Clock::MonoCoarse>(5ms);
    TestClock<dlsm::Clock::GetTimeOfDay>(1ms);

    EXPECT_GT(dlsm::Clock::cputicks(), 0ULL);
}
