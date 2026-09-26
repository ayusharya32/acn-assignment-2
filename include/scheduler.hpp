#pragma once

#include <pthread.h>
#include <queue>

#include "request.hpp"

extern std::deque<AdmittedRequest> schedulerQueue;
extern pthread_mutex_t schedulerQueueMutex;
extern pthread_cond_t schedulerQueueSignal;

AdmittedRequest selectNextRequestToProcess(const std::string &scheduler);