#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include "config.hpp"
#include "util.hpp"
#include "request.hpp"

using namespace std;

void handleRequests(int serverSocket);

int main() {
    Config config = loadConfig("config.json");

    /**
     * 1. Creating Server Socket
     * AF_INET: IPv4
     * SOCK_STREAM: TCP
     * 0: Use default protocol(TCP) as given in param 2
     */
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(config.server.port);

    //To convert server ip, from string to suitable network format (unsigner int)
    //and put it into serverAddress.sin_addr
    inet_pton(AF_INET, config.server.ipAddress.c_str(), &serverAddress.sin_addr);

    /**
     * 2. Binding Server Socket and Server Address
    */
    bind(serverSocket, (sockaddr *) &serverAddress, sizeof(serverAddress));

    /**
     * 3. Making serverSocket as a listening socket and handling requests
    */
    listen(serverSocket, 5);
    handleRequests(serverSocket);

    return 0;

}

void handleRequests(int serverSocket) {
     while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        char buffer[1024] = {0};

        recv(clientSocket, buffer, sizeof(buffer), 0);

        std::string requestString = buffer;
        ParseResult result = parseRequest(requestString);
        auto& request = result.request;

        cout << "Type:"<< request.type << endl << "FileName:" << request.fileName << endl 
        << "Size:" << request.bytes << endl << endl;

        close(clientSocket);
     }
}

