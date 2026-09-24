#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "config.hpp"

void setupClientSocketAndSendRequest(const Config &config, const std::string &requestString);

int main()
{
    Config config = loadConfig("config.json");
    
    setupClientSocketAndSendRequest(config, "get notes.txt\n");
    setupClientSocketAndSendRequest(config, "put notes.txt 512\n");
    setupClientSocketAndSendRequest(config, "health\n");

    return 0;
}

void setupClientSocketAndSendRequest(const Config &config, const std::string &requestString) {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(config.server.port);
    inet_pton(AF_INET, config.server.ipAddress.c_str(), &serverAddress.sin_addr);

    connect(clientSocket, (struct sockaddr*)&serverAddress,
            sizeof(serverAddress));

    
    send(clientSocket, requestString.c_str(), requestString.size(), 0);

    close(clientSocket);
}