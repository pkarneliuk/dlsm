#include "impl/Str.h"
#include "unit/Unit.h"

TEST(Str, ParseOpts) {
    using Opts = dlsm::Str::ParseOpts;

    Opts opts{"opt1=value1,opt2=value2"};
    EXPECT_EQ(opts.Get("opt1"), "value1");
    EXPECT_EQ(opts.Get("opt2"), "value2");
    EXPECT_EQ(opts.Get("opt3"), "");
    EXPECT_EQ(opts.Get("opt3", "default"), "default");

    EXPECT_THAT([] { Opts{"opt=value1,opt=value2"}; }, ThrowsMessage<std::invalid_argument>("Duplicate key:opt"))
        << "Duplicate keys";
}
