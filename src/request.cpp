#include "request.hpp"
#include "util.hpp"
#include <sstream>
#include <cctype>

ParseResult parseRequest(const std::string &requestString) {
    ParseResult result{};
    auto& request = result.request;

    // Splits string at whitespaces into a stream of tokens
    std::stringstream requestStream(requestString);

    requestStream >> request.type;
    request.type = toUpper(request.type);

    requestStream >> request.fileName;
    
    std::string byteCountString;
    requestStream >> byteCountString;

    if (request.type.empty()) {
        result.errorMessage = "Malformed request";
        return result;
    }

    if (!validRequestType(request.type)) {
        result.errorMessage = "Unknown request type";
        return result;
    }

    if (request.type == "GET" || request.type == "PUT") {
        if (request.fileName.empty()) {
            result.errorMessage = "Missing filename";
            return result;
        }

        if (!validFileName(request.fileName)) {
            result.errorMessage = "Invalid filename";
            return result;
        }
    }

    if (request.type == "GET") {
        if (!byteCountString.empty()) {
            result.errorMessage = "Malformed request";
            return result;
        }
    }

    if (request.type == "PUT") {
        if (byteCountString.empty()) {
            result.errorMessage = "Missing byte count";
            return result;
        }

        // Byte count must contain only digits
        for (char c : byteCountString) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                result.errorMessage = "Invalid byte count";
                return result;
            }
        }

        request.bytes = std::stoull(byteCountString);

        std::string extra;
        if (requestStream >> extra) {
            result.errorMessage = "Malformed request";
            return result;
        }
    }

    if (request.type == "HEALTH") {
        if (!request.fileName.empty() || !byteCountString.empty()) {
            result.errorMessage = "Malformed request";
            return result;
        }
    }

    if (request.type == "GET" || request.type == "HEALTH") {
        std::string extra;

        if (requestStream >> extra) {
            result.errorMessage = "Malformed request";
            return result;
        }
    }

    result.success = true;
    return result;
}

bool validRequestType(const std::string &type) {
    return type == "GET" || type == "PUT" || type == "HEALTH";
} 

bool validFileName(const std::string &fileName) {
    return fileName.find('/') == std::string::npos && fileName != "." && fileName != "..";
}