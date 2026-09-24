#pragma once

#include <string>

struct HeaderResult {
    std::string header;
    std::string extra;
    bool success = false;
    std::string errorMessage;
};

HeaderResult readRequestHeader(int clientSocket);
