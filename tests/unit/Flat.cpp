#include <iostream>

#include "fbs/base_generated.h"
#include "fbs/dlsm_generated.h"
#include "unit/Unit.hpp"

TEST(Flat, Simplest) {
    flatbuffers::FlatBufferBuilder builder(1024);
    EXPECT_EQ(builder.GetSize(), 0);
    using namespace dlsm::base;
    auto state = builder.CreateStruct(State{1234ULL, Source::MDFH, Status::Inactive});

    EXPECT_EQ(builder.GetSize(), 16);

    builder.Finish(state);

    EXPECT_EQ(builder.GetSize(), 24);
    EXPECT_NE(builder.GetBufferPointer(), nullptr);
}
