#include "cli.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace std;

int passed = 0;
int failed = 0;

bool runTest(
    const string& name,
    const vector<string>& arguments,
    bool expectedResult
) {
    vector<char*> argv;

    for (const string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }

    ServerOptions options;

    bool result = parseServerArguments(
        static_cast<int>(argv.size()),
        argv.data(),
        options
    );

    if (result == expectedResult) {
        cout << "[PASS] " << name << endl;
        passed++;
        return true;
    }

    cout << "[FAIL] " << name << endl;
    failed++;
    return false;
}

int main() {

    cout << "================================" << endl;
    cout << "       CLI TEST SUITE" << endl;
    cout << "================================" << endl;
    cout << endl;


    // -----------------------------
    // Required arguments
    // -----------------------------

    runTest(
        "Missing --sched",
        {"server", "--file", "./files"},
        false
    );

    runTest(
        "Missing --file",
        {"server", "--sched", "fcfs"},
        false
    );


    // -----------------------------
    // Missing argument values
    // -----------------------------

    runTest(
        "Missing value for --sched",
        {"server", "--sched"},
        false
    );

    runTest(
        "Missing value for --file",
        {"server", "--file"},
        false
    );

    runTest(
        "Missing value for --quantum",
        {"server", "--sched", "rr", "--quantum"},
        false
    );

    runTest(
        "Missing value for --p",
        {"server", "--sched", "fcfs", "--file", "./files", "--p"},
        false
    );

    runTest(
        "Missing value for --config",
        {"server", "--sched", "fcfs", "--file", "./files", "--config"},
        false
    );

    runTest(
        "Missing value for --metrics-out",
        {
            "server",
            "--sched", "fcfs",
            "--file", "./files",
            "--metrics-out"
        },
        false
    );


    // -----------------------------
    // Valid schedulers
    // -----------------------------

    runTest(
        "Valid FCFS",
        {"server", "--sched", "fcfs", "--file", "./files"},
        true
    );

    runTest(
        "Valid SJF",
        {"server", "--sched", "sjf", "--file", "./files"},
        true
    );

    runTest(
        "Valid RR",
        {
            "server",
            "--sched", "rr",
            "--quantum", "4096",
            "--file", "./files"
        },
        true
    );

    runTest(
        "Valid DRR",
        {
            "server",
            "--sched", "drr",
            "--quantum", "4096",
            "--file", "./files"
        },
        true
    );


    // -----------------------------
    // Quantum requirements
    // -----------------------------

    runTest(
        "RR without --quantum",
        {"server", "--sched", "rr", "--file", "./files"},
        false
    );

    runTest(
        "DRR without --quantum",
        {"server", "--sched", "drr", "--file", "./files"},
        false
    );

    runTest(
        "--quantum with FCFS",
        {
            "server",
            "--sched", "fcfs",
            "--quantum", "4096",
            "--file", "./files"
        },
        false
    );

    runTest(
        "--quantum with SJF",
        {
            "server",
            "--sched", "sjf",
            "--quantum", "4096",
            "--file", "./files"
        },
        false
    );


    // -----------------------------
    // Scheduler validation
    // -----------------------------

    runTest(
        "Invalid scheduler",
        {"server", "--sched", "abc", "--file", "./files"},
        false
    );

    runTest(
        "Uppercase scheduler",
        {"server", "--sched", "FCFS", "--file", "./files"},
        false
    );


    // -----------------------------
    // Quantum validation
    // -----------------------------

    runTest(
        "Quantum = 0",
        {
            "server",
            "--sched", "rr",
            "--quantum", "0",
            "--file", "./files"
        },
        false
    );

    runTest(
        "Negative quantum",
        {
            "server",
            "--sched", "rr",
            "--quantum", "-5",
            "--file", "./files"
        },
        false
    );

    runTest(
        "Non-numeric quantum",
        {
            "server",
            "--sched", "rr",
            "--quantum", "abc",
            "--file", "./files"
        },
        false
    );

    runTest(
        "Partially numeric quantum",
        {
            "server",
            "--sched", "rr",
            "--quantum", "4096abc",
            "--file", "./files"
        },
        false
    );

    runTest(
        "Decimal quantum",
        {
            "server",
            "--sched", "rr",
            "--quantum", "4.5",
            "--file", "./files"
        },
        false
    );

    runTest(
        "Quantum overflow",
        {
            "server",
            "--sched", "rr",
            "--quantum",
            "999999999999999999999999999999",
            "--file", "./files"
        },
        false
    );


    // -----------------------------
    // Packetization validation
    // -----------------------------

    runTest(
        "--p = 0",
        {
            "server",
            "--sched", "fcfs",
            "--file", "./files",
            "--p", "0"
        },
        false
    );

    runTest(
        "Negative --p",
        {
            "server",
            "--sched", "fcfs",
            "--file", "./files",
            "--p", "-5"
        },
        false
    );

    runTest(
        "Non-numeric --p",
        {
            "server",
            "--sched", "fcfs",
            "--file", "./files",
            "--p", "abc"
        },
        false
    );

    runTest(
        "Partially numeric --p",
        {
            "server",
            "--sched", "fcfs",
            "--file", "./files",
            "--p", "4abc"
        },
        false
    );

    runTest(
        "Decimal --p",
        {
            "server",
            "--sched", "fcfs",
            "--file", "./files",
            "--p", "4.5"
        },
        false
    );

    runTest(
        "Valid --p = 1",
        {
            "server",
            "--sched", "fcfs",
            "--file", "./files",
            "--p", "1"
        },
        true
    );

    runTest(
        "--p overflow",
        {
            "server",
            "--sched", "fcfs",
            "--file", "./files",
            "--p", "999999999999999999999999999999"
        },
        false
    );


    // -----------------------------
    // Unknown arguments
    // -----------------------------

    runTest(
        "Unknown argument",
        {
            "server",
            "--sched", "fcfs",
            "--file", "./files",
            "--unknown"
        },
        false
    );


    // -----------------------------
    // Arguments in different order
    // -----------------------------

    runTest(
        "Arguments in different order",
        {
            "server",
            "--file", "./files",
            "--p", "4",
            "--metrics-out", "metrics.csv",
            "--sched", "rr",
            "--config", "config.json",
            "--quantum", "4096"
        },
        true
    );


    // -----------------------------
    // All options
    // -----------------------------

    runTest(
        "All options",
        {
            "server",
            "--sched", "rr",
            "--quantum", "4096",
            "--file", "./files",
            "--p", "4",
            "--config", "config.json",
            "--metrics-out", "metrics.csv"
        },
        true
    );


    // -----------------------------
    // Summary
    // -----------------------------

    cout << endl;
    cout << "================================" << endl;
    cout << passed << " tests passed" << endl;
    cout << failed << " tests failed" << endl;
    cout << "================================" << endl;

    if (failed == 0) {
        cout << "ALL CLI TESTS PASSED" << endl;
        return 0;
    }

    cout << "SOME CLI TESTS FAILED" << endl;
    return 1;
}