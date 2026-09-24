#include "util.hpp"
#include <iostream>
#include <cctype>
#include <algorithm>

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