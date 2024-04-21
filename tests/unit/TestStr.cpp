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

TEST(Str, xpair) {
    EXPECT_THAT([&] { dlsm::Str::xpair(""); }, ThrowsMessage<std::invalid_argument>("Invalid value:'' for xpair()"));
    EXPECT_THAT([&] { dlsm::Str::xpair("AxB"); },
                ThrowsMessage<std::invalid_argument>("Invalid value:'AxB' for xpair()"));
    EXPECT_THAT([&] { dlsm::Str::xpair("0x-1"); },
                ThrowsMessage<std::invalid_argument>("Invalid value:'0x-1' for xpair()"));
    EXPECT_THAT([&] { dlsm::Str::xpair("0x0 "); },
                ThrowsMessage<std::invalid_argument>("Invalid value:'0x0 ' for xpair()"));
    EXPECT_THAT([&] { dlsm::Str::xpair("1x0x5"); },
                ThrowsMessage<std::invalid_argument>("Invalid value:'1x0x5' for xpair()"));

    EXPECT_EQ(dlsm::Str::xpair("0x0"), (dlsm::Str::XPair{0, 0}));
    EXPECT_EQ(dlsm::Str::xpair("0x066"), (dlsm::Str::XPair{0, 66}));
    EXPECT_EQ(dlsm::Str::xpair("16x200"), (dlsm::Str::XPair{16, 200}));
}

TEST(Str, Flat) {
    {
        dlsm::Str::Flat<> empty;
        EXPECT_EQ(empty.size(), 16);
        EXPECT_TRUE(empty.str().empty());
    }

    EXPECT_THAT([&] { dlsm::Str::Flat<4>{std::string_view{"1234"}}; },
                ThrowsMessage<std::invalid_argument>("Str::copy(): dst has not enough capacity: 4 to store: 1234"));
    {
        dlsm::Str::Flat<4> flat = std::string_view{"123"};
        EXPECT_EQ(flat.size(), 4);
        EXPECT_EQ(flat.str(), "123");

        flat = std::string_view{"AB"};
        EXPECT_EQ(flat.str(), "AB");
    }
}

namespace TestStr {
struct TestStruct;
}

TEST(Str, typenameOf) {
    using namespace TestStr;
    static_assert(dlsm::Str::typenameOf<int>() == "int");
    static_assert(dlsm::Str::typenameOf<std::vector<TestStruct>>() == "std::vector<TestStr::TestStruct>");
}
