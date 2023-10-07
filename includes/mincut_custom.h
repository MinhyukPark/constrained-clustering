#ifndef MINCUT_H
#define MINCUT_H

#include <algorithms/global_mincut/cactus/cactus_mincut.h>
#include <igraph/igraph.h>


class MinCutCustom {
    public:
        MinCutCustom(const igraph_t* graph) : graph(graph) {
        };
        int ComputeMinCut();
        const std::vector<int>& GetInPartition() const;
        const std::vector<int>& GetOutPartition() const;
    private:
        std::vector<int> in_partition;
        std::vector<int> out_partition;
        std::map<int, int> new_to_old_node_id_map;
        const igraph_t* graph;
};

#endif
