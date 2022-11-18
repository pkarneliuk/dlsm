#include "unit/Unit.hpp"

TEST(Usan, DISABLED_Overflow) {
    int k = 0x7fffffff;
    (void)k;
    ++k;
}