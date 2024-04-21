#include "impl/Memory.hpp"
#include "unit/Unit.hpp"

using namespace dlsm::Memory;

TEST(Memory, alignmentOf) {
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(0UL)), 0);
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(1UL)), 1);
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(2UL)), 2);
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(3UL)), 1);
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(4UL)), 4);
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(5UL)), 1);
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(6UL)), 2);
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(7UL)), 1);
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(8UL)), 8);
    EXPECT_EQ(alignmentOf(reinterpret_cast<void*>(64UL)), 64);
}

TEST(Memory, checkAlignment) {
    int value = 0;
    EXPECT_THAT([&] { checkAlignment(&value, 3); },
                ThrowsMessage<std::invalid_argument>("Alignment must be power of two:3"));

    int* pointer = reinterpret_cast<int*>(1);
    EXPECT_THAT([&] { checkAlignment(pointer, 64); },
                ThrowsMessage<std::runtime_error>("Adderss: 0x1 (alignment:1) of 'int' is not aligned to:64"));

    EXPECT_NO_THROW(checkAlignment(&value));
}
