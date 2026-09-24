#include <iostream>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

#include "framing.hpp"

using namespace std;

void runTest(
    const string& testName,
    const vector<string>& chunks,
    const string& expectedHeader,
    const string& expectedLeftover,
    bool closeSender = false
) {
    int sockets[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1) {
        cerr << "Failed to create socket pair\n";
        return;
    }

    // Send all chunks.
    for (const string& chunk : chunks) {
        send(sockets[0], chunk.data(), chunk.size(), 0);
    }

    // For EOF tests, close the sending side so recv() eventually returns 0.
    if (closeSender) {
        shutdown(sockets[0], SHUT_WR);
    }

    HeaderResult result = readRequestHeader(sockets[1]);

    bool passed =
        result.success &&
        result.header == expectedHeader &&
        result.extra == expectedLeftover;

    if (passed) {
        cout << "[PASS] " << testName << endl;
    } else {
        cout << "[FAIL] " << testName << endl;

        cout << "  Expected success: "
             << true << endl;

        cout << "  Actual success:   "
             << result.success << endl;

        cout << "  Expected header:   ["
             << expectedHeader << "]" << endl;

        cout << "  Actual header:     ["
             << result.header << "]" << endl;

        cout << "  Expected leftover: ["
             << expectedLeftover << "]" << endl;

        cout << "  Actual leftover:   ["
             << result.extra << "]" << endl;

        if (!result.errorMessage.empty()) {
            cout << "  Error: "
                 << result.errorMessage << endl;
        }
    }

    close(sockets[0]);
    close(sockets[1]);
}

void runFailureTest(
    const string& testName,
    const vector<string>& chunks
) {
    int sockets[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1) {
        cerr << "Failed to create socket pair\n";
        return;
    }

    for (const string& chunk : chunks) {
        send(sockets[0], chunk.data(), chunk.size(), 0);
    }

    // Simulate client closing before a complete header arrives.
    shutdown(sockets[0], SHUT_WR);

    HeaderResult result = readRequestHeader(sockets[1]);

    if (!result.success) {
        cout << "[PASS] " << testName << endl;
    } else {
        cout << "[FAIL] " << testName << endl;
        cout << "  Expected framing failure" << endl;
        cout << "  But readRequestHeader() succeeded" << endl;
        cout << "  Header: [" << result.header << "]" << endl;
    }

    close(sockets[0]);
    close(sockets[1]);
}

int main() {

    // ------------------------------------------------------------
    // Normal framing
    // ------------------------------------------------------------

    runTest(
        "Complete header",
        {"GET file.txt\n"},
        "GET file.txt\n",
        ""
    );

    runTest(
        "Header split across sends",
        {"GET fi", "le.txt\n"},
        "GET file.txt\n",
        ""
    );

    runTest(
        "Header split into many sends",
        {"G", "ET ", "file", ".txt", "\n"},
        "GET file.txt\n",
        ""
    );

    // ------------------------------------------------------------
    // Header + body
    // ------------------------------------------------------------

    runTest(
        "Header with leftover body",
        {"PUT file.txt 10\nHello"},
        "PUT file.txt 10\n",
        "Hello"
    );

    runTest(
        "Header and body split across sends",
        {"PUT file.txt ", "10\nHel", "lo"},
        "PUT file.txt 10\n",
        "Hello"
    );

    runTest(
        "Header with complete body",
        {"PUT file.txt 5\nHello"},
        "PUT file.txt 5\n",
        "Hello"
    );

    // ------------------------------------------------------------
    // EOF / incomplete headers
    // ------------------------------------------------------------

    runFailureTest(
        "Connection closes before newline",
        {"GET file.txt"}
    );

    runFailureTest(
        "Empty connection",
        {""}
    );

    runFailureTest(
        "Partial header before connection closes",
        {"GET fi"}
    );

    // ------------------------------------------------------------
    // Header edge cases
    // ------------------------------------------------------------

    runTest(
        "Empty header line",
        {"\n"},
        "\n",
        ""
    );

    runTest(
        "Header followed by extra data",
        {"GET file.txt\nEXTRA"},
        "GET file.txt\n",
        "EXTRA"
    );

    runTest(
        "Header followed by multiple lines of extra data",
        {"GET file.txt\nSECOND\nTHIRD\n"},
        "GET file.txt\n",
        "SECOND\nTHIRD\n"
    );

    // ------------------------------------------------------------
    // Large leftover data
    // ------------------------------------------------------------

    runTest(
        "Header with large leftover body",
        {"PUT file.txt 5\nHelloWorldExtraData"},
        "PUT file.txt 5\n",
        "HelloWorldExtraData"
    );

    return 0;
}