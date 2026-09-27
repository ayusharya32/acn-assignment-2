#pragma once

#include <string>
#include <cstdint>

struct Request {
    std::string type;
    std::string fileName;
    size_t bytes = 0;
};

struct AdmittedRequest {
    int clientSocket; 
    Request request;
    std::string extra;

    size_t totalBytesToTransfer = 0;
    size_t bytesTransferred = 0;
    size_t deficit = 0;

    bool responseStarted = false;

    timespec arrivalTime{};
    timespec startTime{};
    timespec finishTime{};

    uint64_t requestId = 0;
    uint64_t rounds = 0;
    uint64_t forfeitedBytes = 0;
};

struct ParseResult {
    Request request;
    bool success = false;
    std::string errorMessage;
};

ParseResult parseRequest(const std::string& requestHeader);
bool validRequestType(const std::string& type);
bool validFileName(const std::string& fileName);