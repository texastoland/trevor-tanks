#include "test_framework.h"

std::vector<TestCase>& allTests() { static std::vector<TestCase> t; return t; }
int g_failures = 0;

int main() {
    for (auto& t : allTests()) { std::printf("RUN  %s\n", t.name); t.fn(); }
    std::printf("%zu tests, %d failure(s)\n", allTests().size(), g_failures);
    return g_failures ? 1 : 0;
}
