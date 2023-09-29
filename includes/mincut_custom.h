#ifndef MINCUT_H
#define MINCUT_H
/* #pragma once */

#include <ext/alloc_traits.h>
#include <omp.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <tuple>

/* #include "algorithms/global_mincut/algorithms.h" */
#include "algorithms/global_mincut/cactus/cactus_mincut.h"
/* #include "cactus_mincut.h" */
/* #include "algorithms/global_mincut/minimum_cut.h" */
/* #include "common/configuration.h" */
/* #include "common/definitions.h" */
/* #include "data_structure/graph_access.h" */
/* #include "data_structure/mutable_graph.h" */
/* #include "io/graph_io.h" */
/* #include "tlx/cmdline_parser.hpp" */
/* #include "tlx/logger.hpp" */
/* #include "tools/random_functions.h" */
/* #include "tools/string.h" */
/* #include "tools/timer.h" */

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
