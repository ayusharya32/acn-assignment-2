#pragma once

#include <string>
#include <string_view>

constexpr const char* ARG_SCHED = "--sched";
constexpr const char* ARG_QUANTUM = "--quantum";
constexpr const char* ARG_FILE = "--file";
constexpr const char* ARG_PACKETIZATION = "--p";
constexpr const char* ARG_CONFIG = "--config";
constexpr const char* ARG_METRICS_OUT = "--metrics-out";

constexpr const char* SCHED_FCFS = "fcfs";
constexpr const char* SCHED_SJF = "sjf";
constexpr const char* SCHED_ROUND_ROBIN = "rr";
constexpr const char* SCHED_DRR = "drr";

constexpr const char* REQUEST_GET = "GET";
constexpr const char* REQUEST_PUT = "PUT";
constexpr const char* REQUEST_HEALTH = "HEALTH";

constexpr const char* RESPONSE_OK = "OK";
constexpr const char* RESPONSE_ERROR = "ERROR";

constexpr const char* STATUS_OK = "OK";
constexpr const char* STATUS_ERROR = "ERROR";

std::string toUpper(std::string_view sv);
bool setReceiveTimeout(int socket, int seconds);
bool parsePositiveInt(const std::string &valueString, int &result);
bool getFileSize(const std::string &filePath, size_t &fileSize);