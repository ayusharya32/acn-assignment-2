#pragma once

#include <string>
#include <string_view>

std::string toUpper(std::string_view sv);
bool setReceiveTimeout(int socket, int seconds);
// timespec getCurrentTime();
// bool sendFile(int socket, const std::string& filePath);
bool parsePositiveInt(const std::string &valueString, int &result);