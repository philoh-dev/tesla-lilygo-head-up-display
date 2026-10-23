#include "test.hpp"

#include <iostream>

int main() {
    int failures = 0;
    for (const auto& test : tesla_headup::test::registry()) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }

    std::cout << tesla_headup::test::registry().size() << " test(s), " << failures
              << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}

