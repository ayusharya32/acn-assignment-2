#include "framing.hpp"

#include <sys/socket.h>

HeaderResult readRequestHeader(int clientSocket) {
    HeaderResult result{};

    char recvBuffer[1024];
    std::string bufferString;

    while(true) {
        ssize_t bytesReceived = recv(clientSocket, recvBuffer, sizeof(recvBuffer), 0);
        if(bytesReceived <= 0) {
            result.errorMessage = "Failed to receive request header";
            return result;
        }

        bufferString.append(recvBuffer, bytesReceived);
        size_t newLinePos = bufferString.find('\n');

        // std::string::npos -> NO POSITION (Not found)
        if (newLinePos != std::string::npos) {
            result.header = bufferString.substr(0, newLinePos + 1);
            result.extra = bufferString.substr(newLinePos + 1);

            result.success = true;
            return result;
        }
    }

}