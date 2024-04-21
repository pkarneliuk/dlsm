#pragma once
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <regex>

#include "unit/Mock.hpp"

using testing::ContainerEq;
using testing::Contains;
using testing::Each;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::EndsWith;
using testing::Eq;
using testing::HasSubstr;
using testing::Pair;
using testing::StartsWith;
using testing::Throws;
using testing::ThrowsMessage;

// Use std::regex instead platform-specific RE engines in testing::MatchesRegex
MATCHER_P(MatchesRegex, pattern, "Must match std::regex: " + std::string(pattern)) {
    return std::regex_match(std::string(arg), std::regex(pattern));
}

namespace Tests::Unit {}  // namespace Tests::Unit
