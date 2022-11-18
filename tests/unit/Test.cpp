#include "dlsm/Test.h"

#include "unit/Unit.h"

TEST(Test, Simplest) {
    EXPECT_EQ(dlsm::Test(5), 1);
    EXPECT_EQ(dlsm::Test(1), 2);
}
