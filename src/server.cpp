#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <queue>
#include <vector>
#include <fstream>
#include <algorithm>

#include "config.hpp"
#include "util.hpp"
#include "request.hpp"
#include "framing.hpp"
#include "response.hpp"
#include "scheduler.hpp"
#include "cli.hpp"
#include "request_handler.hpp"

using namespace std;

/**
 * Queue used to maintain accepted clients for header parsing on Parsing Thread
 */
queue<int> connectionQueue;
pthread_mutex_t connectionQueueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t connectionQueueSignal = PTHREAD_COND_INITIALIZER;

constexpr int HEADER_TIMEOUT_SECONDS = 5;

void handleRequests(int serverSocket, const Config &config, ServerOptions &options);
void* parserThreadFunc(void* arg);
void* workerThreadFunc(void* arg);

bool getAdmittedRequestTotalBytesToTransfer(const Request& request, 
    const ServerOptions& options, size_t &totalBytesToTransfer);

void enqueueConnection(int clientSocket);
void enqueueAdmittedRequest(const AdmittedRequest &admittedRequest);

int main(int argc, char* argv[]) {
    ServerOptions options{};
    if(!parseServerArguments(argc, argv, options)) return 1;

    Config config = loadConfig("config.json");

    /**
     * 1. Creating Server Socket
     * AF_INET: IPv4
     * SOCK_STREAM: TCP
     * 0: Use default protocol(TCP) as given in param 2
     */
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create server socket" << endl;
        return 1;
    }

    int reuse = 1;
    int reuseAddrResult = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (reuseAddrResult < 0) {
        cerr << "Failed to set SO_REUSEADDR" << endl;
        close(serverSocket);
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(config.server.port);

    //To convert server ip, from string to suitable network format (unsigner int)
    //and put it into serverAddress.sin_addr
    int conversionResult = inet_pton(
        AF_INET, 
        config.server.ipAddress.c_str(), 
        &serverAddress.sin_addr
    );

    if (conversionResult <= 0) {
        cerr << "Invalid server IP address" << endl;
        close(serverSocket);
        return 1;
    }

    /**
     * 2. Binding Server Socket and Server Address
    */
    int bindResult = bind(serverSocket, (sockaddr *) &serverAddress, sizeof(serverAddress));
    if (bindResult < 0) {
        cerr << "Failed to bind server socket" << endl;
        close(serverSocket);
        return 1;
    }

    /**
     * 3. Making serverSocket as a listening socket and handling requests
    */
    int listenResult = listen(serverSocket, 5);
    if (listenResult < 0) {
        cerr << "Failed to listen on server socket" << endl;
        close(serverSocket);
        return 1;
    }

    cout << "Server listening on "
         << config.server.ipAddress
         << ":"
         << config.server.port
         << endl;

    handleRequests(serverSocket, config, options);
    close(serverSocket);

    return 0;
}

void handleRequests(int serverSocket, const Config &config, ServerOptions &options) {
    pthread_t parserThread;
    int threadCreationResult = pthread_create(&parserThread, nullptr, 
        parserThreadFunc, &options);

    if(threadCreationResult != 0) {
        cerr << "Failed to create parser thread" << endl;
        return;
    }

    vector<pthread_t> workerThreads(config.server.serverThreads);
    for(int i=0; i<config.server.serverThreads; i++) {
        threadCreationResult = pthread_create(&workerThreads[i], nullptr, 
            workerThreadFunc, &options);

        if (threadCreationResult != 0) {
            cerr << "Failed to create worker thread " << i << endl;
            return;
        }
    }

     while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            continue;
        }

        cout << "[ACCEPT] fd=" << clientSocket << endl;

        if (!setReceiveTimeout(clientSocket, HEADER_TIMEOUT_SECONDS)) {
            cerr << "Failed to set receive timeout" << endl;
            close(clientSocket);
            continue;
        }

        enqueueConnection(clientSocket);
     }

     // Not reachable currently because the server runs forever.
     pthread_join(parserThread, nullptr);
}

void* parserThreadFunc(void* arg) {
    ServerOptions &options = *static_cast<ServerOptions*>(arg);

    while(true) {
        /**
         * Take mutex lock on connectionQueue and get clientSocket and first position
         */
        pthread_mutex_lock(&connectionQueueMutex);

        while(connectionQueue.empty()) {
            pthread_cond_wait(&connectionQueueSignal, &connectionQueueMutex);
        }

        int clientSocket = connectionQueue.front();
        connectionQueue.pop();

        cout << "[PARSER_START] fd=" << clientSocket << endl;

        pthread_mutex_unlock(&connectionQueueMutex);

        /**
         * Start header recv and parsing on accessed clientSocket
         */
        HeaderResult headerResult = readRequestHeader(clientSocket);

        if (!headerResult.success) {
            cout << "[PARSER_ERROR] fd=" << clientSocket
                << " reason=" << headerResult.errorMessage
                << endl;

            sendErrorResponse(clientSocket, headerResult.errorMessage);
            close(clientSocket);
            continue;
        }

        /**
         * Parse received header
         */
        ParseResult requestParseResult = parseRequest(headerResult.header);

        if (!requestParseResult.success) {
            sendErrorResponse(clientSocket, requestParseResult.errorMessage);
            close(clientSocket);
            continue;
        }

        cout << "[PARSER_SUCCESS] fd=" << clientSocket
            << " type=" << requestParseResult.request.type
            << endl;

        /**
         * Process requests after successful parsing
         */
        auto& request = requestParseResult.request;

        if(request.type == REQUEST_HEALTH) {
            serveHealthRequestImmediately(clientSocket);
            continue;
        }

        size_t totalBytesToTransfer = 0;
        if(!getAdmittedRequestTotalBytesToTransfer(request, 
            options, totalBytesToTransfer)) {
                sendErrorResponse(
                    clientSocket,
                    request.type == "GET"
                        ? "file does not exist"
                        : "invalid request size"
                );

            close(clientSocket);
            continue;
        }

        AdmittedRequest admittedRequest;
        admittedRequest.clientSocket = clientSocket;
        admittedRequest.request = requestParseResult.request;
        admittedRequest.totalBytesToTransfer = totalBytesToTransfer;
        admittedRequest.extra = headerResult.extra;

        clock_gettime(CLOCK_MONOTONIC, &admittedRequest.arrivalTime);
        enqueueAdmittedRequest(admittedRequest);
    }

    return nullptr;
}

void* workerThreadFunc(void* arg) {
    ServerOptions &options = *static_cast<ServerOptions*>(arg);

    while(true) {
        pthread_mutex_lock(&schedulerQueueMutex);

        while(schedulerQueue.empty()) {
            pthread_cond_wait(&schedulerQueueSignal, &schedulerQueueMutex);
        }

        AdmittedRequest request = selectNextRequestToProcess(options.scheduler);

        pthread_mutex_unlock(&schedulerQueueMutex);

        serveRequest(request, options);
        if (request.bytesTransferred < request.totalBytesToTransfer) {
            enqueueAdmittedRequest(request);
        }
    }

    return nullptr;
}

bool getAdmittedRequestTotalBytesToTransfer(const Request& request, 
    const ServerOptions& options, size_t &totalBytesToTransfer) {

    if(request.type == REQUEST_GET) {
        std::string filePath = options.fileDirectory + "/" + request.fileName;
        size_t fileSize = 0;

        if(!getFileSize(filePath, fileSize)) {
            return false;
        }

        totalBytesToTransfer = fileSize;
        return true;
    }

    if(request.type == REQUEST_PUT) {
        totalBytesToTransfer = request.bytes;
        return true;
    }

    return false;
}

void enqueueConnection(int clientSocket) {
    pthread_mutex_lock(&connectionQueueMutex);
    connectionQueue.push(clientSocket);

    cout << "[QUEUE] fd=" << clientSocket
        << " queue_size=" << connectionQueue.size()
        << endl;

    pthread_mutex_unlock(&connectionQueueMutex);

    //Signaling parser thread to wake up
    pthread_cond_signal(&connectionQueueSignal);
}

void enqueueAdmittedRequest(const AdmittedRequest& admittedRequest){
    pthread_mutex_lock(&schedulerQueueMutex);

    schedulerQueue.push_back(admittedRequest);

    pthread_mutex_unlock(&schedulerQueueMutex);
    pthread_cond_signal(&schedulerQueueSignal);
}


