#include "impl/Str.hpp"
#include "unit/Unit.hpp"

TEST(Str, ParseOpts) {
    using Opts = dlsm::Str::ParseOpts;

    Opts opts{"opt1=value1,opt2=value2"};
    EXPECT_EQ(opts.get("opt1"), "value1");
    EXPECT_EQ(opts.get("opt2"), "value2");
    EXPECT_EQ(opts.get("opt3", "default"), "default");
    EXPECT_THAT([&] { opts.required("opt3"); },
                ThrowsMessage<std::invalid_argument>("Missing required key=value for key:opt3"));

    EXPECT_THAT([] { Opts{"opt=value1,opt=value2"}; }, ThrowsMessage<std::invalid_argument>("Duplicate key:opt"))
        << "Duplicate keys";
}

TEST(Str, split) {
    EXPECT_EQ(dlsm::Str::split("", ","), (std::vector<std::string>{""}));
    EXPECT_EQ(dlsm::Str::split("a,b,c", ","), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(dlsm::Str::split("a--b-c", "--"), (std::vector<std::string>{"a", "b-c"}));
    EXPECT_EQ(dlsm::Str::split("a--b-c", ""), (std::vector<std::string>{"a--b-c"}));
}
