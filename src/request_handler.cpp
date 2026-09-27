#include <algorithm>
#include <cstddef>
#include <fstream>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "request_handler.hpp"
#include "request.hpp"
#include "response.hpp"
#include "scheduler.hpp"
#include "cli.hpp"
#include "util.hpp"

void serveHealthRequestImmediately(int clientSocket) {
    pthread_mutex_lock(&schedulerQueueMutex);

    size_t queueDepth = schedulerQueue.size();

    pthread_mutex_unlock(&schedulerQueueMutex);
    
    sendOkResponse(clientSocket, queueDepth);
    close(clientSocket);
}

bool serveRequest(AdmittedRequest &request, const ServerOptions& options) {
    if (request.request.type == REQUEST_GET) {
        return serveGetRequest(request, options);

    } else if (request.request.type == REQUEST_PUT) {
        return servePutRequest(request, options);
    }
}

bool serveGetRequest(AdmittedRequest& request, const ServerOptions& options) {
    bool sendSizeSuccess = sendOkResponse(request.clientSocket, request.totalBytesToTransfer);
    if(!sendSizeSuccess) {
        close(request.clientSocket);
        return false;
    }

    std::string filePath = options.fileDirectory + "/" + request.request.fileName;
    std::ifstream file(filePath, std::ios::binary);

    if(!file) {
        sendErrorResponse(request.clientSocket, "Unable to open file");
        close(request.clientSocket);
        return false;
    }

    char buffer[8192];
    while(file && request.bytesTransferred < request.totalBytesToTransfer) {
        size_t remainingBytesToTranfer = request.totalBytesToTransfer - request.bytesTransferred;
        size_t bytesToRead = std::min(sizeof(buffer), remainingBytesToTranfer);

        file.read(buffer, bytesToRead);
        std::streamsize bytesRead = file.gcount();

        if(bytesRead <= 0) {
            sendErrorResponse(request.clientSocket, "Failed to read file");
            close(request.clientSocket);
            return false;
        }

        if(!sendAllBytes(request.clientSocket, buffer, static_cast<size_t>(bytesRead))) {
            close(request.clientSocket);
            return false;
        }

        request.bytesTransferred += static_cast<size_t>(bytesRead);
    }

    close(request.clientSocket);
    return true;
}

bool servePutRequest(AdmittedRequest &request, const ServerOptions &options) {
    bool sendOkSuccess = sendOkResponse(request.clientSocket, 0);
    if(!sendOkSuccess) {
        close(request.clientSocket);
        return false;
    }
    
    std::string filePath = options.fileDirectory + "/" + request.request.fileName;
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);

    if(!file) {
        sendErrorResponse(request.clientSocket, "Unable to open file");
        close(request.clientSocket);
        return false;
    }

    if (!request.extra.empty()) {
        size_t extraBytesToWrite = std::min(request.extra.size(),request.totalBytesToTransfer);
        file.write(request.extra.data(), extraBytesToWrite);

        if (!file) {
            sendErrorResponse(request.clientSocket,"Failed to write file");
            close(request.clientSocket);
            return false;
        }
        request.bytesTransferred += extraBytesToWrite;
    }


    char buffer[8192];
    while(request.bytesTransferred < request.totalBytesToTransfer) {
        size_t remainingBytesToTranfer = request.totalBytesToTransfer - request.bytesTransferred;
        size_t bytesToReceive = std::min(sizeof(buffer), remainingBytesToTranfer);

        ssize_t bytesActuallyReceived = recv(request.clientSocket, buffer, bytesToReceive, 0);
        if(bytesActuallyReceived <= 0) {
            sendErrorResponse(request.clientSocket, "Unable to receive file");
            close(request.clientSocket);
            return false;
        }

        file.write(buffer, bytesActuallyReceived);
        request.bytesTransferred += static_cast<size_t>(bytesActuallyReceived);
    }

    file.close();

    if(!sendOkResponse(request.clientSocket, 0)) {
        close(request.clientSocket);
        return false;
    }

    close(request.clientSocket);
    return true;
}
