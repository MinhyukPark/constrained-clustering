#ifndef CONSTRAINED_H
#define CONSTRAINED_H
/* #pragma once */
#include "mincut_custom.h"

#include <chrono>

#include <libleidenalg/GraphHelper.h>
#include <libleidenalg/Optimiser.h>
#include <libleidenalg/CPMVertexPartition.h>
#include <libleidenalg/ModularityVertexPartition.h>

class ConstrainedClustering {
    public:
        ConstrainedClustering(std::string edgelist, std::string algorithm, double resolution, int num_processors, std::string output_file, std::string log_file, int log_level) : edgelist(edgelist), algorithm(algorithm), resolution(resolution), num_processors(num_processors), output_file(output_file), log_file(log_file), log_level(log_level) {
            if(this->log_level > 0) {
                this->start_time = std::chrono::steady_clock::now();
                this->log_file_handle.open(this->log_file);
            }
        };
        virtual int main() = 0;
        int write_to_log_file(std::string message, int message_type);
        void write_partition_map(std::map<int,int>& final_partition);
        void start_workers(std::vector<std::map<int,int>>& results, igraph_t* graph);
        virtual ~ConstrainedClustering() {
            if(this->log_level > 0) {
                this->log_file_handle.close();
            }
        }
    protected:
        std::string edgelist;
        std::string algorithm;
        double resolution;
        int num_processors;
        std::string output_file;
        std::string log_file;
        std::chrono::steady_clock::time_point start_time;
        std::ofstream log_file_handle;
        int log_level;
};

class MincutGlobalClusterRepeat : public ConstrainedClustering {
    public:
        MincutGlobalClusterRepeat(std::string edgelist, std::string algorithm, double resolution, int num_processors, std::string output_file, std::string log_file, int log_level) : ConstrainedClustering(edgelist, algorithm, resolution, num_processors, output_file, log_file, log_level) {
        };
        int main();
};

#endif
