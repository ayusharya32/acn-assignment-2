#pragma once

#include <string>

struct ServerOptions {
    std::string scheduler;
    int quantum = 0;
    std::string fileDirectory;
    int packetization = 1;
    std::string configPath = "config.json";
    std::string metricsOutput = "metrics.csv";
};

bool parseServerArguments(int argc, char* argv[], ServerOptions &options);