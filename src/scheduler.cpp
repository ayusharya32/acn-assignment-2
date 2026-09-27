#include "scheduler.hpp"
#include "util.hpp"

std::deque<AdmittedRequest> schedulerQueue;

pthread_mutex_t schedulerQueueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t schedulerQueueSignal = PTHREAD_COND_INITIALIZER;

AdmittedRequest selectNextRequestToProcess(const std::string &scheduler) {
    AdmittedRequest request;

    if(scheduler == SCHED_FCFS || scheduler == SCHED_ROUND_ROBIN || scheduler == SCHED_DRR) {
        request = schedulerQueue.front();
        schedulerQueue.pop_front();

    } else if(scheduler == SCHED_SJF) {
        auto shortest = schedulerQueue.begin();

        for(auto it = schedulerQueue.begin(); it != schedulerQueue.end(); it++) {
            if(it->totalBytesToTransfer < shortest->totalBytesToTransfer) {
                shortest = it;
            }
        }

        request = *shortest;
        schedulerQueue.erase(shortest);
    }

    return request;
}