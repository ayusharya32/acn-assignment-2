#include "response.hpp"
#include <sys/socket.h>

bool sendResponse(int clientSocket, const std::string &responseString){
    size_t totalBytesSent = 0;

    while(totalBytesSent < responseString.size()) {
        ssize_t bytesSent = send(
            clientSocket, 
            &responseString[0] + totalBytesSent,
            responseString.size() - totalBytesSent,
            0
        );

        if(bytesSent <= 0) {
            return false;
        }

        totalBytesSent += bytesSent;
    }

    return true;
}

bool sendOkResponse(int clientSocket, size_t value){
    std::string responseString = std::string("OK ") + std::to_string(value) + "\n";
    return sendResponse(clientSocket, responseString);
}

bool sendErrorResponse(int clientSocket, const std::string &errorMessage){
    std::string responseString = std::string("ERR ") + errorMessage+ "\n";
    return sendResponse(clientSocket, responseString);
}

bool sendAllBytes(int clientSocket, const char *data, size_t bytes) {
    size_t totalSent = 0;

    while(totalSent < bytes) {
        ssize_t bytesActuallySent = send(clientSocket, data + totalSent, bytes - totalSent, 0);

        if(bytesActuallySent <= 0) {
            return false;
        }

        totalSent += static_cast<size_t>(bytesActuallySent);
    }

    return true;
}
