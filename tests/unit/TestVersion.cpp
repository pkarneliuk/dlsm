#include <type_traits>

#include "dlsm/Version.hpp"
#include "unit/Unit.hpp"

TEST(Version, Constants) {
    namespace V = dlsm::Version;
    EXPECT_THAT(V::build_type, MatchesRegex("(release|debug|relwithdebinfo)"));
    EXPECT_THAT(V::build_time, MatchesRegex(R"(^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$)"));
    EXPECT_THAT(V::commit, MatchesRegex(R"(^(unknown|[0-9a-fA-F]{40} \d{4}-\d\d-\d\d \d\d:\d\d:\d\d.*)$)"));
    EXPECT_THAT(V::string, MatchesRegex(R"(^\d+\.\d+\.\d+.\d+$)"));

    EXPECT_TRUE(std::is_unsigned_v<decltype(V::number)>);
}
