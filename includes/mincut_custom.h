#ifndef MINCUT_H
#define MINCUT_H

#include <algorithms/global_mincut/cactus/cactus_mincut.h>
#include "algorithms/global_mincut/minimum_cut.h"
#include <igraph/igraph.h>
#include <string>


class MinCutCustom {
    public:
        MinCutCustom(const igraph_t* graph, const std::string& mincut_type = "cactus") : graph(graph), mincut_type(mincut_type) {
        };
        int ComputeMinCut();
        /* int ComputeAllMinCuts(); */
        const std::vector<int>& GetInPartition() const;
        const std::vector<int>& GetOutPartition() const;
        /* const std::vector<std::vector<int>>& GetAllPartitions() const; */
    private:
        std::vector<int> in_partition;
        std::vector<int> out_partition;
        /* std::vector<std::vector<int>> all_partitions; */
        std::map<int, int> new_to_old_node_id_map;
        const igraph_t* graph;
        std::string mincut_type;
};

#endif
