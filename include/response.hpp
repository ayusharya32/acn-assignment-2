#pragma once

#include <string>
#include <cstddef>

bool sendResponse(int clientSocket, const std::string &responseString);
bool sendOkResponse(int clientSocket, size_t fileSize);
bool sendErrorResponse(int clientSocket, const std::string &errorMessage);
bool sendAllBytes(int clientSocket, const char *data, size_t bytes);