#include <latch>
#include <thread>

#include "impl/Lock.hpp"
#include "unit/Unit.hpp"

template<typename T, typename ... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

TEST(Lock, LockUnlockTryLock) {
    const auto test = [](auto s) {
        EXPECT_NO_THROW({ s.lock(); });
        EXPECT_NO_THROW({ s.unlock(); });

        EXPECT_TRUE(s.try_lock());
        if(IsAnyOf<decltype(s), dlsm::Lock::None, dlsm::Lock::StdRutex>) {
            EXPECT_TRUE(s.try_lock());
            EXPECT_NO_THROW(s.unlock(););
        }
        else EXPECT_FALSE(s.try_lock());
        EXPECT_NO_THROW(s.unlock(););

        std::jthread t([&] { EXPECT_TRUE(s.try_lock()); s.unlock(); });
    };

    test(dlsm::Lock::None{});
    test(dlsm::Lock::TASS{});
    test(dlsm::Lock::TTSS{});
    test(dlsm::Lock::StdMutex{});
    test(dlsm::Lock::StdRutex{});
}
