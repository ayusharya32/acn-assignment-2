#pragma once

#include <string>

struct Request {
    std::string type;
    std::string fileName;
    size_t bytes = 0;
};

struct ParseResult {
    Request request;
    bool success = false;
    std::string errorMessage;
};

ParseResult parseRequest(const std::string& requestHeader);
bool validRequestType(const std::string& type);
bool validFileName(const std::string& fileName);