#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <filesystem>
#include <atomic>
#include <thread>
#include <random>

#include "config.hpp"
#include "response.hpp"
#include "util.hpp"
#include "cli.hpp"

struct BufferedSocket
{
    int socket;
    std::string buffer;
};

struct LoadState
{
    const Config *config;
    const std::vector<std::string> *workloadFiles;
    int totalRequests;

    std::atomic<int> nextRequest{0};
    std::atomic<bool> failed{false};
};

int connectToServer(const Config &config);
bool sendString(int socket, const std::string &message);

bool readHeader(BufferedSocket &connection, std::string &line);
bool readBodyData(BufferedSocket &connection, char *bodyData, size_t totalBodySize);

bool parseOkResponse(const std::string &line, size_t &value);
bool parseErrorResponse(const std::string &line, std::string &reason);

bool openInputFile(const std::string &path, std::ifstream &file);

std::string extractFileName(const std::string &filePath);
std::string buildPutRequest(const std::string &filePath, size_t fileSize, const std::string &destinationName = "");
bool putFile(const Config &config, const std::string &filePath, const std::string &destinationName = "");

std::string buildGetRequest(const std::string &fileName);
bool getFile(const Config &config, const std::string &filePath, bool saveToDisk = true);

std::vector<std::string> getWorkloadFiles(const std::string &directory);
bool seedWorkload(const Config &config, const std::vector<std::string> &workloadFiles);
void loadWorker(LoadState &state);

bool runLoad(const Config &config,
             const std::vector<std::string> &workloadFiles,
             int totalRequests);

int main(int argc, char *argv[]){
    ClientOptions options;

    if (!parseClientArguments(argc, argv, options)) {
        return 1;
    }

    Config config;
    try {
        config = loadConfig(options.configPath);
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (options.command == "put") {
        if (!putFile(config, options.path)) {
            return 1;
        }

        return 0;
    }

    if (options.command == "get") {
        if (!getFile(config, options.path)) {
            return 1;
        }

        return 0;
    }

    if (options.command == "load") {
        std::vector<std::string> workloadFiles =
            getWorkloadFiles(options.path);

        if (workloadFiles.empty()) {
            std::cerr << "error: workload directory is empty or invalid\n";
            return 1;
        }

        if (!seedWorkload(config, workloadFiles)) {
            std::cerr << "error: workload seeding failed\n";
            return 1;
        }

        if (!runLoad(config, workloadFiles, options.requests)) {
            return 1;
        }

        return 0;
    }

    return 1;
}

int connectToServer(const Config &config) {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(config.server.port);

    if (inet_pton(AF_INET, config.server.ipAddress.c_str(), &serverAddress.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address\n";
        close(clientSocket);
        return -1;
    }

    if (connect(clientSocket, reinterpret_cast<sockaddr *>(&serverAddress), 
            sizeof(serverAddress)) < 0) {
        perror("connect");
        close(clientSocket);
        return -1;
    }

    return clientSocket;
}

bool sendString(int socket, const std::string &message) {
    return sendAllBytes(socket, message.c_str(), message.size());
}

bool readHeader(BufferedSocket &connection, std::string &line) {
    while (true) {
        // Read first line from buffer (header) and leave the rest
        size_t newlinePosition = connection.buffer.find('\n');
        if (newlinePosition != std::string::npos) {
            line = connection.buffer.substr(0, newlinePosition + 1);
            connection.buffer.erase(0, newlinePosition + 1);

            return true;
        }

        // If first line is not received yet or partially received
        char temp[4096];
        ssize_t bytesReceived = recv(connection.socket, temp, sizeof(temp), 0);

        if (bytesReceived <= 0) {
            return false;
        }

        connection.buffer.append(temp, bytesReceived);
    }
}

bool readBodyData(BufferedSocket &connection, char *bodyData, size_t totalBodySize) {
    size_t totalRead = 0;

    while(totalRead < totalBodySize) {
        if(!connection.buffer.empty()) {
            size_t extraDataSize = connection.buffer.size();
            size_t bytesYetToRead = totalBodySize - totalRead;
            size_t bytesToCopy = std::min(extraDataSize, bytesYetToRead);

            std::memcpy(bodyData + totalRead, connection.buffer.data(), bytesToCopy);
            connection.buffer.erase(0, bytesToCopy);
            totalRead += bytesToCopy;
            continue;
        }

        ssize_t bytesActuallyReceived = recv(connection.socket, 
            bodyData + totalRead, totalBodySize - totalRead, 0);

        if(bytesActuallyReceived <= 0) {
            return false;
        }

        totalRead += static_cast<size_t>(bytesActuallyReceived);
    }

    return true;
}

bool parseOkResponse(const std::string &line, size_t &value) {
    if (line.rfind("OK ", 0) != 0) {
        return false;
    }

    std::string number = line.substr(3);

    if (!number.empty() && number.back() == '\n') {
        number.pop_back();
    }

    try {
        size_t parsedCharacters = 0;
        unsigned long long parsedValue = std::stoull(number, &parsedCharacters);
        if (parsedCharacters != number.size()) {
            return false;
        }

        value = static_cast<size_t>(parsedValue);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool parseErrorResponse(const std::string &line, std::string &reason){
    if (line.rfind("ERR ", 0) != 0) {
        return false;
    }

    reason = line.substr(4);

    if (!reason.empty() && reason.back() == '\n') {
        reason.pop_back();
    }

    return !reason.empty();
}

bool openInputFile(const std::string &path, std::ifstream &file) {
    file.open(path, std::ios::binary);
    return file.is_open();
}

std::string extractFileName(const std::string &filePath) {
    size_t slashPosition = filePath.find_last_of("/\\");
    if (slashPosition == std::string::npos) {
        return filePath;
    }
    return filePath.substr(slashPosition + 1);
}

std::string buildPutRequest(const std::string &filePath, size_t fileSize, const std::string &destinationName) {
    std::string fileName = destinationName.empty() ? extractFileName(filePath) : destinationName;
    return "PUT " + fileName + " " + std::to_string(fileSize) + "\n";
}

bool putFile(const Config &config, const std::string &filePath, const std::string &destinationName){
    std::ifstream file;

    if (!openInputFile(filePath, file)) {
        std::cerr << "Unable to open file: " << filePath << '\n';
        return false;
    }

    size_t fileSize;

    if (!getFileSize(filePath, fileSize)) {
        std::cerr << "Unable to get file size: " << filePath << '\n';
        return false;
    }

    std::string request = buildPutRequest(filePath, fileSize, destinationName);

    int clientSocket = connectToServer(config);

    if (clientSocket < 0) {
        return false;
    }

    if (!sendString(clientSocket, request)) {
        close(clientSocket);
        return false;
    }

    BufferedSocket connection{clientSocket, {}};

    std::string responseLine;

    if (!readHeader(connection, responseLine)) {
        close(clientSocket);
        return false;
    }

    size_t responseValue;
    if (parseOkResponse(responseLine, responseValue)) {
        if (responseValue != 0) {
            close(clientSocket);
            return false;
        }

        file.seekg(0);
        char buffer[4096];

        while (file) {
            file.read(buffer, sizeof(buffer));
            std::streamsize bytesRead = file.gcount();

            if (bytesRead <= 0) {
                break;
            }

            if (!sendAllBytes(clientSocket, buffer, static_cast<size_t>(bytesRead))) {
                close(clientSocket);
                return false;
            }
        }

        std::string finalResponseLine;

        if (!readHeader(connection, finalResponseLine)) {
            close(clientSocket);
            return false;
        }

        if (!parseOkResponse(finalResponseLine, responseValue) ||
            responseValue != 0) {

            std::string errorReason;
            if (parseErrorResponse(finalResponseLine, errorReason)) {
                std::cerr << "PUT failed: " << errorReason << '\n';
            } else {
                std::cerr << "PUT failed: unexpected response: "
                        << finalResponseLine;
            }

            close(clientSocket);
            return false;
        }

        close(clientSocket);
        return true;

    } else {
        std::string errorReason;
        if (parseErrorResponse(responseLine, errorReason)) {
            std::cerr << "PUT failed: " << errorReason << '\n';
        }

        close(clientSocket);
        return false;
    }

    return true;
}

std::string buildGetRequest(const std::string &fileName) {
    return "GET " + extractFileName(fileName) + "\n";
}

bool getFile(const Config &config, const std::string &filePath, bool saveToDisk){
    int clientSocket = connectToServer(config);
    if (clientSocket < 0) {
        return false;
    }

    std::string fileName = extractFileName(filePath);
    std::string request = buildGetRequest(fileName);

    if (!sendString(clientSocket, request)) {
        close(clientSocket);
        return false;
    }

    BufferedSocket connection{clientSocket, {}};

    std::string responseLine;

    if (!readHeader(connection, responseLine)) {
        close(clientSocket);
        return false;
    }

    std::string errorReason;
    if (parseErrorResponse(responseLine, errorReason)) {
        std::cerr << "GET failed: " << errorReason << '\n';
        close(clientSocket);
        return false;
    }

    size_t fileSize;
    if (!parseOkResponse(responseLine, fileSize)) {
        close(clientSocket);
        return false;
    }

    std::vector<char> fileData(fileSize);
    if (!readBodyData(connection, fileData.data(), fileSize)) {
        close(clientSocket);
        return false;
    }

    if (saveToDisk) {
        std::ofstream outputFile(fileName, std::ios::binary);

        if (!outputFile.is_open()) {
            close(clientSocket);
            return false;
        }

        outputFile.write(fileData.data(), fileSize);

        if (!outputFile) {
            close(clientSocket);
            return false;
        }

        outputFile.close();
    }

    close(clientSocket);
    return true;
}

std::vector<std::string> getWorkloadFiles(const std::string &directory)
{
    std::vector<std::string> files;

    std::error_code error;

    if (!std::filesystem::is_directory(directory, error)) {
        return files;
    }

    for (const auto &entry :
         std::filesystem::directory_iterator(directory, error)) {

        if (error) {
            return {};
        }

        if (entry.is_regular_file()) {
            files.push_back(entry.path().string());
        }
    }

    return files;
}

bool seedWorkload(const Config &config,
                  const std::vector<std::string> &workloadFiles)
{
    for (const std::string &filePath : workloadFiles) {
        if (!putFile(config, filePath)) {
            std::cerr << "Seeding failed for: " << filePath << '\n';
            return false;
        }
    }

    return true;
}

void loadWorker(LoadState &state)
{
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());

    std::uniform_int_distribution<size_t> fileDistribution(
        0,
        state.workloadFiles->size() - 1
    );

    std::uniform_int_distribution<int> operationDistribution(0, 1);

    while (true) {
        int requestNumber = state.nextRequest.fetch_add(1);

        if (requestNumber >= state.totalRequests) {
            break;
        }

        const std::string &filePath =
            (*state.workloadFiles)[fileDistribution(generator)];

        bool success;

        if (operationDistribution(generator) == 0) {
            success = getFile(*state.config, filePath, false);
        } else {
            std::string destFileName = "load_put_" + std::to_string(requestNumber) + "_" + extractFileName(filePath);
            success = putFile(*state.config, filePath, destFileName);
        }

        if (!success) {
            state.failed.store(true);
        }
    }
}

bool runLoad(const Config &config,
             const std::vector<std::string> &workloadFiles,
             int totalRequests)
{
    LoadState state{
        &config,
        &workloadFiles,
        totalRequests
    };

    std::vector<std::thread> workers;

    for (int i = 0; i < config.server.clientThreads; i++) {
        workers.emplace_back(loadWorker, std::ref(state));
    }

    for (std::thread &worker : workers) {
        worker.join();
    }

    return !state.failed.load();
}