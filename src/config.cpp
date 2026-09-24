#include "config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

Config loadConfig(const std::string &filePath) {
    /**
     * ifstream: Taking data from file
     * ofstream: Putting data onto the file
     */
    std::ifstream file(filePath);

    if(!file) {
        throw std::runtime_error("Unable to open config file");
    }

    json configJson;
    file >> configJson;

    Config config;
    
    config.server.ipAddress = configJson["server"]["ip"];
    config.server.port = configJson["server"]["port"].get<int>();
    config.server.serverThreads = configJson["server"]["server_threads"].get<int>();
    config.server.clientThreads = configJson["server"]["client_threads"].get<int>();

    config.loadBalancer.ipAddress = configJson["load_balancer"]["ip"];
    config.loadBalancer.port = configJson["load_balancer"]["port"].get<int>();
    config.loadBalancer.healthIntervalMs = configJson["load_balancer"]["health_interval_ms"].get<int>();

    for (const auto &backend: configJson["load_balancer"]["backends"]) {
        config.loadBalancer.backends.push_back(BackendConfig{
            backend["ip"],
            backend["port"]
        });
    }

    return config;
}