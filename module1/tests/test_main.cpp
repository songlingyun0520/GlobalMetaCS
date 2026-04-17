#include "test_harness.h"

int main() {
    const char* filter = std::getenv("TEST_FILTER");
    int failed = 0;
    int ran = 0;

    for (const auto& test : TestRegistry()) {
        if (filter != nullptr && test.name.find(filter) == std::string::npos) {
            continue;
        }
        ++ran;
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& ex) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << "\n";
        }
    }

    if (ran == 0) {
        std::cerr << "No tests matched filter\n";
        return 1;
    }
    return failed == 0 ? 0 : 1;
}
