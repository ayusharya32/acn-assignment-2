#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "config.hpp"

using namespace std;

int main() {
    Config config = loadConfig("config.json");

    /**
     * AF_INET: IPv4
     * SOCK_STREAM: TCP
     * 0: Use default protocol(TCP) as given in param 2
     */
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(config.server.port);

    /**
     * To convert server ip, from string to suitable network format (unsigner int)
     * and put it into serverAddress.sin_addr
     */
    inet_pton(AF_INET, config.server.ipAddress.c_str(), &serverAddress.sin_addr);

    bind(serverSocket, (sockaddr *) &serverAddress, sizeof(serverAddress));

    listen(serverSocket, 5);

    int clientSocket = accept(serverSocket, nullptr, nullptr);
    char buffer[1024] = {0};

    recv(clientSocket, buffer, sizeof(buffer), 0);
    cout << "Message from client: " << buffer
              << endl;

    // closing the socket.
    close(serverSocket);

    return 0;

}