#include "impl/Logger.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <regex>

#include "impl/Str.hpp"

namespace {

template <typename Mutex>
class LoggerSink final : public spdlog::sinks::base_sink<Mutex> {
private:
    dlsm::Logger::ISink& sink_;

public:
    explicit LoggerSink(dlsm::Logger::ISink& sink) : sink_{sink} {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override try {
        spdlog::memory_buf_t record;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, record);
        sink_.AddRecord(fmt::to_string(record));
    } catch (const std::exception& e) {
        sink_.AddRecord("Exception on formatting raw message:" + std::string(msg.payload.data()) +
                        " exception:" + e.what());
    }

    void flush_() override {}
};

using LoggerSink_sink_mt = LoggerSink<std::mutex>;
using LoggerSink_sink_st = LoggerSink<spdlog::details::null_mutex>;
}  // namespace

namespace dlsm {

Logger::Logger(const std::string& config, const std::string& format, Logger::ISink* sink) {
    std::smatch res;
    static const std::regex re(R"(^(\w+):(\w+):(.*)$)");
    if (!std::regex_match(config.cbegin(), config.cend(), res, re)) {
        throw std::invalid_argument("Unexpected logger config value:" + config +
                                    " expected format: <level>:<sink>:opt1=a,opt2=b");
    }

    const auto level = res[1].str();
    const auto type = res[2].str();
    const auto opts = dlsm::Str::ParseOpts(res[3].str());

    const auto opt = [&config, &opts](const std::string& key, const std::string& default_ = {}) -> std::string {
        try {
            return opts.get(key, default_);
        } catch (const std::exception& e) {
            throw std::invalid_argument("Logger config: '" + config + "' error: " + e.what());
        }
    };

    const auto make = [&opt, sink](auto& type) -> spdlog::sink_ptr {
        if (type == "null")
            return std::make_shared<spdlog::sinks::null_sink_mt>();
        else if (type == "stdout")
            return std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        else if (type == "stderr")
            return std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        else if (type == "file") {
            const auto truncate = opt("truncate", "default") == "on";
            return std::make_shared<spdlog::sinks::basic_file_sink_mt>(opt("path"), truncate);
        } else if (type == "rotating") {
            const auto max_size = std::stol(opt("max_size"));
            const auto max_files = std::stol(opt("max_files"));
            return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(opt("path"), max_size, max_files, false);
        } else if (type == "sink") {
            if (sink == nullptr) throw std::invalid_argument("Pointer to requested logger sink is nullptr");
            return std::make_shared<LoggerSink_sink_mt>(*sink);
        }
        throw std::invalid_argument("Unexpected logger sink value:" + type);
    };

    const auto name = opt("name", "default");
    if (opt("async", "default") == "on") {
        spdlog::init_thread_pool(10240, 1);
        auto tp = spdlog::thread_pool();
        log_ = std::make_shared<spdlog::async_logger>(name, make(type), tp);
    } else {
        log_ = std::make_shared<spdlog::logger>(name, make(type));
    }

    log_->set_level(spdlog::level::from_str(level));
    if (log_->level() == spdlog::level::off && level != "off") {
        throw std::invalid_argument("Unexpected logging level value:" + level);
    }

    const auto flush = opt("flush", level);
    log_->flush_on(spdlog::level::from_str(flush));
    if (log_->flush_level() == spdlog::level::off && flush != "off") {
        throw std::invalid_argument("Unexpected logging flush level value:" + flush);
    }

    bool useUtc = opt("time", "default") != "local";
    const auto time = useUtc ? spdlog::pattern_time_type::utc : spdlog::pattern_time_type::local;
    std::string tz = useUtc ? "Z" : "";
    std::string loc = (log_->level() <= spdlog::level::debug) ? "%s:%# %!" : "";

    // Default pattern is(for debug and trace levels):
    // 2023-02-16 18:23:46:32.651Z PID TID level name foo.cpp:123 func message
    auto pattern = format.empty() ? "%Y-%m-%d %T.%e" + tz + " %P %t %l %n " + loc + " %v" : format;
    log_->set_pattern(pattern, time);

    const auto save = opt("register", "off");
    if (save != "off") {
        if (save == "default")
            spdlog::set_default_logger(log_);
        else
            spdlog::register_logger(log_);
    }
}

Logger::Logger(Logger& that, const std::string& name, bool registration) : log_{that->clone(name)} {
    if (registration) spdlog::register_logger(log_);
}

Logger Logger::get(const std::string& name) {
    auto result = name.empty() ? spdlog::default_logger() : spdlog::get(name);
    if (!result) throw std::runtime_error("Unregistered logger name:" + name);
    return Logger{result};
}

}  // namespace dlsm