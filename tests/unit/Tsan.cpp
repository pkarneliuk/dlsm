#include <mutex>
#include <thread>

#include "unit/Unit.hpp"

TEST(Tsan, DISABLED_Deadlock) {
    std::mutex m;
    m.lock();
    m.lock();
}

TEST(Tsan, DISABLED_DataRace) {
    int v = 0;
    auto t = std::thread([&] { v = 2; });
    v += 5;
    t.join();
}