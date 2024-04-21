#pragma once
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <mutex>

#include "impl/Logger.hpp"

namespace Tests::Unit {

struct LoggingSink final : public dlsm::Logger::ISink, std::vector<std::string> {
    std::mutex _records;
    void AddRecord(std::string&& record) noexcept override {  // NOLINT
        std::scoped_lock guard(_records);
        this->emplace_back(record);
    }
    const std::string& tail(std::size_t index = 0) {
        std::scoped_lock guard(_records);
        EXPECT_LT(index, this->size());
        return *(this->rbegin() + static_cast<std::ptrdiff_t>(index));
    }
};

inline LoggingSink& Log() {
    static LoggingSink sink;
    return sink;
}
}  // namespace Tests::Unit