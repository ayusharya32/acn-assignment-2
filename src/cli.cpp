#include "cli.hpp"

#include <iostream>
#include <string>
#include "util.hpp"

using namespace std;

constexpr const char* ARG_SCHED = "--sched";
constexpr const char* ARG_QUANTUM = "--quantum";
constexpr const char* ARG_FILE = "--file";
constexpr const char* ARG_PACKETIZATION = "--p";
constexpr const char* ARG_CONFIG = "--config";
constexpr const char* ARG_METRICS_OUT = "--metrics-out";

constexpr const char* SCHED_FCFS = "fcfs";
constexpr const char* SCHED_SJF  = "sjf";
constexpr const char* SCHED_ROUND_ROBIN = "rr";
constexpr const char* SCHED_DRR  = "drr";

bool nextIndexValid(int argc, int currentIndex);
bool loadArgValuesInServerOptions(int argc, char* argv[], ServerOptions &options, 
    bool &schedulerProvided, bool &fileProvided, bool &quantumProvided);

bool parseServerArguments(int argc, char* argv[], ServerOptions &options) {
    bool schedulerProvided = false;
    bool fileProvided = false;
    bool quantumProvided = false;

    bool loadSuccess = loadArgValuesInServerOptions(argc, argv, options, 
        schedulerProvided, fileProvided, quantumProvided);    

    if(!loadSuccess) {
        return false;
    }

    if (!schedulerProvided) {
        cerr << "error: missing required argument " << ARG_SCHED << endl;
        return false;
    }

    if (!fileProvided) {
        cerr << "error: missing required argument " << ARG_FILE << endl;
        return false;
    }

    if (options.scheduler != SCHED_FCFS && options.scheduler != SCHED_SJF &&
            options.scheduler != SCHED_ROUND_ROBIN && options.scheduler != SCHED_DRR) {

        cerr << "Error: invalid --sched value: " << options.scheduler << endl;
        return false;
    }

    bool quantumRequired = options.scheduler == SCHED_ROUND_ROBIN 
        || options.scheduler == SCHED_DRR;

    if(quantumRequired && !quantumProvided) {
        cerr << "error: --quantum is required for "<< options.scheduler << endl;
        return false;
    }

    if(!quantumRequired && quantumProvided) {
        cerr << "error: --quantum is only valid for rr or drr" << endl;
        return false;
    }

    return true;
}  

bool loadArgValuesInServerOptions(int argc, char* argv[], ServerOptions &options, 
    bool &schedulerProvided, bool &fileProvided, bool &quantumProvided) {

    for(int i=1; i<argc; i++) {
        std::string argument = argv[i];

        if(argument == ARG_SCHED) {
            if(!nextIndexValid(argc, i)) {
                cerr << "Error: missing value for " << argument << endl;
                return false;
            }

            options.scheduler = argv[++i];
            schedulerProvided = true;

        } else if(argument == ARG_QUANTUM) {
            if(!nextIndexValid(argc, i)) {
                cerr << "Error: missing value for " << argument << endl;
                return false;
            }

            string quantumString =  argv[++i];
            int quantum;

            if(!parsePositiveInt(quantumString, quantum)) {
                cerr << "error: invalid --quantum" << endl;
                return false;
            }

            options.quantum = quantum;
            quantumProvided = true;

        } else if(argument == ARG_FILE) {
            if(!nextIndexValid(argc, i)) {
                cerr << "Error: missing value for " << argument << endl;
                return false;
            }

            options.fileDirectory = argv[++i];
            fileProvided = true;

        } else if(argument == ARG_PACKETIZATION) {
            if(!nextIndexValid(argc, i)) {
                cerr << "Error: missing value for " << argument << endl;
                return false;
            }

            string packetizationString =  argv[++i];
            int packetization;

            if(!parsePositiveInt(packetizationString, packetization)) {
                cerr << "error: invalid --p" << endl;
                return false;
            }

            options.packetization = packetization;
        } else if(argument == ARG_CONFIG) {
            if(!nextIndexValid(argc, i)) {
                cerr << "Error: missing value for " << argument << endl;
                return false;
            }

            options.configPath = argv[++i];

        } else if(argument == ARG_METRICS_OUT) {
            if(!nextIndexValid(argc, i)) {
                cerr << "Error: missing value for " << argument << endl;
                return false;
            }

            options.metricsOutput = argv[++i];

        } else {
            cerr << "Error: unknown argument " << argument << endl;
            return false;
        }
    }

    return true;
}

bool nextIndexValid(int argc, int currentIndex) {
    return (currentIndex + 1) < argc;
}