#pragma once

#include <exception>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tesla_headup::test {

using TestFunction = void (*)();

struct TestCase {
    std::string name;
    TestFunction function;
};

inline auto registry() -> std::vector<TestCase>& {
    static std::vector<TestCase> tests;
    return tests;
}

class Registration {
public:
    Registration(std::string name, TestFunction function) {
        registry().push_back({std::move(name), function});
    }
};

inline void fail(
    const char* expression,
    const char* file,
    int line,
    const std::string& detail = {}) {
    std::ostringstream message;
    message << file << ':' << line << ": check failed: " << expression;
    if (!detail.empty()) {
        message << " (" << detail << ')';
    }
    throw std::runtime_error(message.str());
}

}  // namespace tesla_headup::test

#define TESLA_HEADUP_CONCAT_IMPL(left, right) left##right
#define TESLA_HEADUP_CONCAT(left, right) TESLA_HEADUP_CONCAT_IMPL(left, right)

#define TEST_CASE(name)                                                        \
    static void TESLA_HEADUP_CONCAT(test_, __LINE__)();                                \
    static ::tesla_headup::test::Registration TESLA_HEADUP_CONCAT(registration_, __LINE__){    \
        name, TESLA_HEADUP_CONCAT(test_, __LINE__)};                                   \
    static void TESLA_HEADUP_CONCAT(test_, __LINE__)()

#define CHECK(expression)                                                      \
    do {                                                                       \
        if (!(expression)) {                                                   \
            ::tesla_headup::test::fail(#expression, __FILE__, __LINE__);               \
        }                                                                      \
    } while (false)

#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        const auto actual_value = (actual);                                    \
        const auto expected_value = (expected);                                \
        if (!(actual_value == expected_value)) {                               \
            ::tesla_headup::test::fail(#actual " == " #expected, __FILE__, __LINE__); \
        }                                                                      \
    } while (false)

