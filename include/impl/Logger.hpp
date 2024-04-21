#pragma once
#ifdef SPDLOG_ACTIVE_LEVEL
#undef SPDLOG_ACTIVE_LEVEL
#endif
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <spdlog/spdlog.h>

#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(log_, __VA_ARGS__)        // NOLINT(cppcoreguidelines-macro-usage)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(log_, __VA_ARGS__)        // NOLINT(cppcoreguidelines-macro-usage)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(log_, __VA_ARGS__)          // NOLINT(cppcoreguidelines-macro-usage)
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(log_, __VA_ARGS__)          // NOLINT(cppcoreguidelines-macro-usage)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(log_, __VA_ARGS__)        // NOLINT(cppcoreguidelines-macro-usage)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(log_, __VA_ARGS__)  // NOLINT(cppcoreguidelines-macro-usage)

namespace dlsm {

class Logger {
    std::shared_ptr<spdlog::logger> log_;

    Logger(std::shared_ptr<spdlog::logger>& impl) : log_{impl} {}

public:
    struct ISink {
        virtual ~ISink() = default;
        virtual void AddRecord(std::string&& record) noexcept = 0;
    };

    Logger(const std::string& config, const std::string& format = {}, ISink* sink = nullptr);
    Logger(Logger& that, const std::string& name, bool registration = false);
    static Logger get(const std::string& name = {});

    inline spdlog::logger* operator->() const { return log_.get(); }
};

}  // namespace dlsm
