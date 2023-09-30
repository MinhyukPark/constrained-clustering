#ifndef MINCUT_H
#define MINCUT_H

#include <algorithms/global_mincut/cactus/cactus_mincut.h>
#include <igraph/igraph.h>

// typedef graph_access graph_type;
typedef mutable_graph graph_type;
typedef std::shared_ptr<graph_type> GraphPtr;

class MinCutCustom {
    public:
        MinCutCustom(igraph_t* graph) : graph(graph) {
        };
        int ComputeMinCut();
        const std::vector<int>& GetInPartition() const;
        const std::vector<int>& GetOutPartition() const;
    private:
        std::vector<int> in_partition;
        std::vector<int> out_partition;
        std::map<int, int> new_to_old_node_id_map;
        igraph_t* graph;
};

#endif
