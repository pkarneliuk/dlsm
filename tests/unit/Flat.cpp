#include <iostream>

#include "fbs/base_generated.h"
#include "fbs/dlsm_generated.h"
#include "unit/Unit.hpp"

TEST(Flat, Simplest) {
    flatbuffers::FlatBufferBuilder builder(1024);
    std::cout << builder.GetSize() << std::endl;
    using namespace dlsm::base;
    auto state = builder.CreateStruct(State{1234ULL, Source::MDFH, Status::Inactive});

    std::cout << builder.GetSize() << std::endl;

    builder.Finish(state);

    auto buff = builder.GetBufferPointer();
    auto size = builder.GetSize();

    std::cout << size << " " << (void*)buff << std::endl;

    GetHeartBeat(buff);

    // base::GetState(bu);
}
