#include "unit/Unit.hpp"

TEST(Asan, DISABLED_DoubleFree) {
    auto p = new char;
    delete p;
    delete p;
}

TEST(Asan, DISABLED_LeakedAllocation) { new char; }