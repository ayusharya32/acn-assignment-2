#include <iostream>
#include "request.hpp"

void test(const std::string& input, bool expected) {
    ParseResult result = parseRequest(input);

    bool passed = result.success == expected;

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << input;

    if (!result.success)
        std::cout << " -> " << result.errorMessage;

    std::cout << '\n';
}

int main() {
    // Valid
    test("GET file.txt", true);
    test("PUT file.txt 100", true);
    test("PUT file.txt 0", true);
    test("HEALTH", true);

    // Empty / malformed
    test("", false);
    test("   ", false);

    // Unknown type
    test("DELETE file.txt", false);
    test("HELLO", false);

    // Missing filename
    test("GET", false);
    test("PUT", false);

    // Invalid filename
    test("GET /etc/passwd", false);
    test("GET .", false);
    test("GET ..", false);
    test("PUT /tmp/a.txt 100", false);

    // Invalid byte count
    test("PUT file.txt", false);
    test("PUT file.txt abc", false);
    test("PUT file.txt -10", false);
    test("PUT file.txt 10abc", false);

    // GET should not have byte count
    test("GET file.txt 100", false);

    // HEALTH should have nothing else
    test("HEALTH file.txt", false);
    test("HEALTH 100", false);

    // Extra header fields
    test("GET file.txt extra", false);
    test("HEALTH extra", false);
}