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

struct ClientOptions {
    std::string command;
    std::string path;
    std::string configPath = "config.json";
    int requests = -1;
};

bool parseServerArguments(int argc, char* argv[], ServerOptions &options);
bool parseClientArguments(int argc, char* argv[], ClientOptions &options);