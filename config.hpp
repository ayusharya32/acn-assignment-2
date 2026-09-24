/**
 * #pragme once: To ensure that this file is included only once in a single compilation
 * vector: Dynamic Array
 */
#pragma once

#include <string>
#include <vector>

struct ServerConfig {
    std::string ipAddress;
    int port;
    int serverThreads;
    int clientThreads;
};

struct BackendConfig {
    std::string ipAddress;
    int port;
};

struct LoadBalancerConfig {
    std::string ipAddress;
    int port;
    int healthIntervalMs;
    std::vector<BackendConfig> backends;
};

struct Config {
    ServerConfig server;
    LoadBalancerConfig loadBalancer;
};

Config loadConfig(const std::string &filePath);