#include <iostream>
#include <string>

#include <sys/socket.h>
#include <unistd.h>

#include "response.hpp"

using namespace std;

void runTest(
    const string& testName,
    bool (*sendFunction)(int),
    const string& expected
) {
    int sockets[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1) {
        cerr << "Failed to create socket pair\n";
        return;
    }

    bool result = sendFunction(sockets[0]);

    string actual;
    char buffer[1024];

    while (true) {
        ssize_t bytesReceived = recv(
            sockets[1],
            buffer,
            sizeof(buffer),
            0
        );

        if (bytesReceived <= 0) {
            break;
        }

        actual.append(buffer, bytesReceived);

        if (actual.size() >= expected.size()) {
            break;
        }
    }

    if (result && actual == expected) {
        cout << "[PASS] " << testName << endl;
    } else {
        cout << "[FAIL] " << testName << endl;
        cout << "  Expected size: " << expected.size() << endl;
        cout << "  Actual size:   " << actual.size() << endl;
        cout << "  Expected: [" << expected << "]" << endl;
        cout << "  Actual:   [" << actual << "]" << endl;
    }

    close(sockets[0]);
    close(sockets[1]);
}

bool testOk5(int socket) {
    return sendOkResponse(socket, 5);
}

bool testOk0(int socket) {
    return sendOkResponse(socket, 0);
}

bool testOkLarge(int socket) {
    return sendOkResponse(socket, 123456789);
}

bool testErrorMalformed(int socket) {
    return sendErrorResponse(socket, "Malformed request");
}

bool testErrorNotFound(int socket) {
    return sendErrorResponse(socket, "File not found");
}

bool testErrorWithSpaces(int socket) {
    return sendErrorResponse(
        socket,
        "Invalid byte count in request"
    );
}

bool testLargeResponse(int socket) {
    string largeResponse(100000, 'A');

    return sendResponse(socket, largeResponse);
}

int main() {
    // Basic OK responses
    runTest(
        "OK response",
        testOk5,
        "OK 5\n"
    );

    runTest(
        "OK zero response",
        testOk0,
        "OK 0\n"
    );

    runTest(
        "OK large number",
        testOkLarge,
        "OK 123456789\n"
    );

    // Basic ERR responses
    runTest(
        "ERR response",
        testErrorMalformed,
        "ERR Malformed request\n"
    );

    runTest(
        "ERR file-not-found response",
        testErrorNotFound,
        "ERR File not found\n"
    );

    runTest(
        "ERR reason with spaces",
        testErrorWithSpaces,
        "ERR Invalid byte count in request\n"
    );

    // Large response tests the send() loop
    runTest(
        "Large response",
        testLargeResponse,
        string(100000, 'A')
    );

    return 0;
}