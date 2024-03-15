#include <latch>
#include <thread>

#include "impl/SpinLock.hpp"
#include "unit/Unit.hpp"

TEST(SpinLock, LockUnlockTryLock) {
    const auto test = [](auto s) {
        EXPECT_NO_THROW({ s.lock(); });
        EXPECT_NO_THROW({ s.unlock(); });
        EXPECT_NO_THROW({ s.unlock(); });
        EXPECT_TRUE(s.try_lock());
        EXPECT_FALSE(s.try_lock());
        EXPECT_NO_THROW(s.unlock(););

        std::jthread t([&] { EXPECT_TRUE(s.try_lock()); });
    };

    test(dlsm::SpinLock::TASSLock{});
    test(dlsm::SpinLock::TTSSLock{});
}
