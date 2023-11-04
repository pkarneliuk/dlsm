#include <spdlog/async.h>

#include "impl/Logger.hpp"
#include "unit/Unit.hpp"

TEST(Logger, Construction) {
    EXPECT_THAT([] { dlsm::Logger(""); },
                ThrowsMessage<std::invalid_argument>(
                    "Unexpected logger config value: expected format: <level>:<sink>:opt1=a,opt2=b"));

    EXPECT_THAT([] { dlsm::Logger("bad:null:"); },
                ThrowsMessage<std::invalid_argument>("Unexpected logging level value:bad"));

    EXPECT_THAT([] { dlsm::Logger("debug:bad:"); },
                ThrowsMessage<std::invalid_argument>("Unexpected logger sink value:bad"));

    EXPECT_THAT([] { dlsm::Logger("debug:null:flush=bad"); },
                ThrowsMessage<std::invalid_argument>("Unexpected logging flush level value:bad"));

    EXPECT_THAT([] { dlsm::Logger("debug:file:"); },
                ThrowsMessage<std::invalid_argument>(
                    "Logger config: 'debug:file:' error: Missing required key=value for key:path"));

    EXPECT_THAT([] { dlsm::Logger("debug:file:truncate=on"); },
                ThrowsMessage<std::invalid_argument>(
                    "Logger config: 'debug:file:truncate=on' error: Missing required key=value for key:path"));

    EXPECT_THAT([] { dlsm::Logger("debug:rotating:"); },
                ThrowsMessage<std::invalid_argument>(EndsWith("Missing required key=value for key:max_size")));

    EXPECT_THAT([] { dlsm::Logger("debug:rotating:path=testing.log,max_size=4096"); },
                ThrowsMessage<std::invalid_argument>(EndsWith("Missing required key=value for key:max_files")));

    EXPECT_THAT([] { dlsm::Logger("debug:rotating:max_size=4096,max_files=2"); },
                ThrowsMessage<std::invalid_argument>(EndsWith("Missing required key=value for key:path")));

    EXPECT_THAT([] { dlsm::Logger("debug:sink:"); },
                ThrowsMessage<std::invalid_argument>("Pointer to requested logger sink is nullptr"));

    EXPECT_NO_THROW(dlsm::Logger("debug:stdout:"););
    EXPECT_NO_THROW(dlsm::Logger("debug:stderr:"););

    EXPECT_NO_THROW(Tests::Unit::LoggingSink records; dlsm::Logger log("trace:sink:async=on", {}, &records);
                    EXPECT_EQ(log->name(), "default");
                    EXPECT_NE(dynamic_cast<spdlog::async_logger*>(log.operator->()), nullptr););

    EXPECT_NO_THROW(
        Tests::Unit::LoggingSink records; dlsm::Logger log_("trace:sink:name=Default", {}, &records);
        LOG_WARN("test message"); EXPECT_THAT(
            records.tail(),
            MatchesRegex(
                R"(^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d.\d{3}Z \d+ \d+ warning Default [\w.]+:\d\d (.)*TestBody test message[\r\n]{1,2}$)")););

    EXPECT_NO_THROW(
        Tests::Unit::LoggingSink records; dlsm::Logger log_("trace:sink:name=Default,time=local", {}, &records);
        LOG_WARN("test message"); EXPECT_THAT(
            records.tail(),
            MatchesRegex(
                R"(^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d.\d{3} \d+ \d+ warning Default [\w.]+:\d\d (.)*TestBody test message[\r\n]{1,2}$)")););

    EXPECT_NO_THROW(
        Tests::Unit::LoggingSink records; dlsm::Logger log_("info:sink:name=Pattern", "[%n][%l] %s:%# %v", &records);
        LOG_INFO("info message");
        EXPECT_THAT(records.tail(), MatchesRegex(R"(^\[Pattern\]\[info\] [\w.]+:\d\d info message[\r\n]{1,2}$)")););
}

TEST(Logger, Macros) {
    Tests::Unit::LoggingSink records;
    dlsm::Logger log_("trace:sink:", "%l %v", &records);
    EXPECT_TRUE(records.empty());
    LOG_TRACE("TRACE");
    EXPECT_THAT(records.tail(), MatchesRegex(R"(^trace TRACE[\r\n]{1,2}$)"));
    LOG_DEBUG("DEBUG");
    EXPECT_THAT(records.tail(), MatchesRegex(R"(^debug DEBUG[\r\n]{1,2}$)"));
    LOG_INFO("INFO");
    EXPECT_THAT(records.tail(), MatchesRegex(R"(^info INFO[\r\n]{1,2}$)"));
    LOG_WARN("WARN");
    EXPECT_THAT(records.tail(), MatchesRegex(R"(^warning WARN[\r\n]{1,2}$)"));
    LOG_ERROR("ERROR");
    EXPECT_THAT(records.tail(), MatchesRegex(R"(^error ERROR[\r\n]{1,2}$)"));
    LOG_CRITICAL("CRITICAL");
    EXPECT_THAT(records.tail(), MatchesRegex(R"(^critical CRITICAL[\r\n]{1,2}$)"));
    EXPECT_EQ(records.size(), 6);
}

TEST(Logger, Get) {
    EXPECT_THAT([] { dlsm::Logger::get("TestNull"); },
                ThrowsMessage<std::runtime_error>("Unregistered logger name:TestNull"));

    EXPECT_NO_THROW({ dlsm::Logger log("trace:null:name=TestNull,register=on", {}); });
    EXPECT_NO_THROW({
        dlsm::Logger log("trace:null:name=Default,register=default", {});
        dlsm::Logger def{dlsm::Logger::get()};
    });

    EXPECT_NO_THROW({
        auto log = dlsm::Logger::get("TestNull");
        EXPECT_EQ(log->name(), "TestNull");

        dlsm::Logger c1(log, "TestClone1", true);
        auto clone = dlsm::Logger::get("TestClone1");
        EXPECT_EQ(clone->name(), "TestClone1");

        dlsm::Logger c2(log, "TestClone2", false);
        EXPECT_THROW({ dlsm::Logger::get("TestClone2"); }, std::runtime_error);
    });
}
