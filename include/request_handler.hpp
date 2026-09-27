#pragma once

#include "request.hpp"
#include "cli.hpp"

void serveHealthRequestImmediately(int clientSocket);
bool serveRequest(AdmittedRequest &request, const ServerOptions& options);
bool serveGetRequest(AdmittedRequest &request, const ServerOptions& options);
bool servePutRequest(AdmittedRequest &request, const ServerOptions& options);
bool sendAllBytes(int clientSocket, const char *data, size_t bytes);