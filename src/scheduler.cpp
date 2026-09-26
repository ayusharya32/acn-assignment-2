#include "scheduler.hpp"
#include "util.hpp"

std::deque<AdmittedRequest> schedulerQueue;

pthread_mutex_t schedulerQueueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t schedulerQueueSignal = PTHREAD_COND_INITIALIZER;

AdmittedRequest selectNextRequestToProcess(const std::string &scheduler) {
    if(scheduler == SCHED_FCFS) {
        AdmittedRequest request = schedulerQueue.front();
        schedulerQueue.pop_front();
        return request;
    }

    if(scheduler == SCHED_SJF) {
        auto shortest = schedulerQueue.begin();

        for(auto it = schedulerQueue.begin(); it != schedulerQueue.end(); it++) {
            if(it->totalBytesToTransfer < shortest->totalBytesToTransfer) {
                shortest = it;
            }
        }

        AdmittedRequest selectedRequest = *shortest;
        schedulerQueue.erase(shortest);
        return selectedRequest;
    }

    // RR/DRR will be implemented next.
    AdmittedRequest request = schedulerQueue.front();
    schedulerQueue.pop_front();
    return request;
}