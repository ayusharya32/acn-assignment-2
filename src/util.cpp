#include "util.hpp"
#include <iostream>
#include <cctype>
#include <algorithm>
#include <sys/socket.h>
#include <sys/time.h>

std::string toUpper(std::string_view originalString) {
    std::string result;
    result.reserve(originalString.size());
    std::transform(
        originalString.begin(), 
        originalString.end(), 
        std::back_inserter(result), 
        [] (unsigned char c) -> char { 
            return std::toupper(c); 
        }
    );

    return result;
}

bool setReceiveTimeout(int socket, int seconds) {
    // Set receive timeout for this socket.
    timeval timeout{};
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;

    return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
}

bool parsePositiveInt(const std::string &valueString, int &result) {
    try {
        size_t charactersConsumed = 0;
        int number = std::stoi(valueString, &charactersConsumed);

        if (charactersConsumed != valueString.size() || number <= 0) {
            return false;
        }

        result = number;
        return true;
    }
    catch (...) {
        return false;
    }
}
