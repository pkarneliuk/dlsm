#include "unit/Unit.hpp"

// NOLINTBEGIN
TEST(Asan, DISABLED_DoubleFree) {
    auto p = new char;
    delete p;
    delete p;
}

TEST(Asan, LeakedAllocation) { new char; }
// NOLINTEND
