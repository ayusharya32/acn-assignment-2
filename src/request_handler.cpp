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

bool serveGetFcfsOrSjfRequest(AdmittedRequest& request, const ServerOptions& options);
bool serveGetRoundRobinRequest(AdmittedRequest& request, const ServerOptions& options);
bool serveGetDeficitRoundRobinRequest(AdmittedRequest& request, const ServerOptions& options);

bool servePutFcfsOrSjfRequest(AdmittedRequest &request, const ServerOptions& options);
bool servePutRoundRobinRequest(AdmittedRequest &request, const ServerOptions& options);
bool servePutDeficitRoundRobinRequest(AdmittedRequest& request, const ServerOptions& options);

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

    } else {
        return servePutRequest(request, options);
    }
}

bool serveGetRequest(AdmittedRequest& request, const ServerOptions& options) {
    if(options.scheduler == SCHED_FCFS || options.scheduler == SCHED_SJF) {
        return serveGetFcfsOrSjfRequest(request, options);
    } else if(options.scheduler == SCHED_ROUND_ROBIN) {
        return serveGetRoundRobinRequest(request, options);
    } else {
        return serveGetDeficitRoundRobinRequest(request, options);
    }
}

bool servePutRequest(AdmittedRequest &request, const ServerOptions &options) {
    if(options.scheduler == SCHED_FCFS || options.scheduler == SCHED_SJF) {
        return servePutFcfsOrSjfRequest(request, options);
    } else if(options.scheduler == SCHED_ROUND_ROBIN) {
        return servePutRoundRobinRequest(request, options);
    } else {
        return servePutDeficitRoundRobinRequest(request, options);
    }

    return false;
}


bool serveGetFcfsOrSjfRequest(AdmittedRequest& request, const ServerOptions& options) {
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

bool serveGetRoundRobinRequest(AdmittedRequest& request, const ServerOptions& options) {
    if(!request.responseStarted) {
        bool sendSizeSuccess = sendOkResponse(request.clientSocket, request.totalBytesToTransfer);
        if(!sendSizeSuccess) {
            close(request.clientSocket);
            return false;
        }

        request.responseStarted = true;
    }

    std::string filePath = options.fileDirectory + "/" + request.request.fileName;
    std::ifstream file(filePath, std::ios::binary);

    if(!file) {
        sendErrorResponse(request.clientSocket, "Unable to open file");
        close(request.clientSocket);
        return false;
    }

    file.seekg(static_cast<std::streamoff>(request.bytesTransferred));

    int quantumRemaining = options.quantum;
    bool isFirstLineOfRound = true;
    while(request.bytesTransferred < request.totalBytesToTransfer) {
        std::string currentLine;

        if(!std::getline(file, currentLine)) {
            sendErrorResponse(request.clientSocket, "Failed to read file");
            close(request.clientSocket);
            return false;
        }

        if (!file.eof()) {
            currentLine.push_back('\n');
        }


        int lineBytes = static_cast<int>(currentLine.size());

        // Handling first line being more than quantum size itself: Overrun
        if( lineBytes > quantumRemaining && isFirstLineOfRound) {
            if (!sendAllBytes(request.clientSocket, currentLine.data(), lineBytes)) {
                close(request.clientSocket);
                return false;
            }

            request.bytesTransferred += currentLine.size();
            break;
        }

        if(lineBytes > quantumRemaining) {
            break;
        }

        if(!sendAllBytes(request.clientSocket, currentLine.data(), currentLine.size())) {
            close(request.clientSocket);
            return false;
        }

        quantumRemaining -= lineBytes;
        request.bytesTransferred += currentLine.size();
        isFirstLineOfRound = false;

        if(quantumRemaining == 0) {
            break;
        }
    }

    file.close();

    if(request.bytesTransferred == request.totalBytesToTransfer) {
        close(request.clientSocket);
    }
    return true;
}

bool servePutFcfsOrSjfRequest(AdmittedRequest &request, const ServerOptions& options) {
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

bool servePutRoundRobinRequest(AdmittedRequest &request, const ServerOptions& options) {
    if(!request.responseStarted) {
        bool sendOkSuccess = sendOkResponse(request.clientSocket, 0);
        if(!sendOkSuccess) {
            close(request.clientSocket);
            return false;
        }
        request.responseStarted = true;
    }

    std::string filePath =options.fileDirectory + "/" + request.request.fileName;
    std::ofstream file;

    if (request.bytesTransferred == 0) {
        file.open(filePath, std::ios::binary | std::ios::trunc);
    } else {
        file.open(filePath, std::ios::binary | std::ios::app);
    }

    if (!file) {
        sendErrorResponse(request.clientSocket, "Unable to open file");
        close(request.clientSocket);
        return false;
    }

    int quantumRemaining = options.quantum;
    if (!request.extra.empty()) {
        size_t extraBytesToWrite = std::min(request.extra.size(), static_cast<size_t>(quantumRemaining));
        file.write(request.extra.data(), extraBytesToWrite);

        if (!file) {
            sendErrorResponse(request.clientSocket,"Failed to write file");
            close(request.clientSocket);
            return false;
        }
        request.bytesTransferred += extraBytesToWrite;
        quantumRemaining -= extraBytesToWrite;
        request.extra.erase(0, extraBytesToWrite);
    }

    char buffer[8192]; 
    while (request.bytesTransferred < request.totalBytesToTransfer && quantumRemaining > 0) {
        size_t remainingRequestBytes = request.totalBytesToTransfer - request.bytesTransferred; 
        size_t bytesToReceive = std::min(
            sizeof(buffer), 
            std::min(remainingRequestBytes, static_cast<size_t>(quantumRemaining))
        );

        ssize_t bytesActuallyReceived = recv(request.clientSocket, buffer, bytesToReceive, 0); 
        if(bytesActuallyReceived <= 0) { 
            sendErrorResponse( request.clientSocket, "Unable to receive file" ); 
            close(request.clientSocket); 
            return false; 
        } 
        
        file.write(buffer, bytesActuallyReceived); 
        
        if(!file) { 
            sendErrorResponse(request.clientSocket, "Failed to write file"); 
            close(request.clientSocket); 
            return false; 
        } 
        
        request.bytesTransferred += static_cast<size_t>(bytesActuallyReceived); 
        quantumRemaining -= static_cast<int>(bytesActuallyReceived);
    }

    if (request.bytesTransferred == request.totalBytesToTransfer) { 
        if(!sendOkResponse(request.clientSocket, 0)) {
            close(request.clientSocket);
            return false; 
        } 
        close(request.clientSocket); 
    }

    return true;
}

bool serveGetDeficitRoundRobinRequest(AdmittedRequest& request, const ServerOptions& options) {
    
}