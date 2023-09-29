#include "mincut_custom.h"

int MinCutCustom::ComputeMinCut() {
    int edge_cut_size = 0;
    auto cfg = configuration::getConfig();
    cfg->algorithm = "cactus";
    cfg->queue_type = "bqueue";
    cfg->find_most_balanced_cut = true;
    cfg->threads = 1;
    cfg->save_cut = true;
    random_functions::setSeed(0);
    std::shared_ptr<graph_type> G = std::make_shared<graph_type>();

    // make input output mapping of node ids
    // make sure graph is connected
    // igraph maybe just always has increasing node ids starting at 0 and also is connected
    int num_nodes = 2;
    int num_edges = 1;
    G->start_construction(num_nodes, num_edges);
    NodeID node_u = G->new_node();
    G->setPartitionIndex(node_u, 0);
    NodeID node_v = G->new_node();
    G->setPartitionIndex(node_v, 0);
    NodeID node_t = G->new_node();
    G->setPartitionIndex(node_t, 0);

    G->new_edge(node_u, node_v, 1);
    G->finish_construction();
    G->computeDegrees();

    /* auto* cmc = selectMincutAlgorithm<GraphPtr>(cfg->algorithm); */
    auto* cmc = new cactus_mincut<GraphPtr>();
    cmc->perform_minimum_cut(G);

    if(G->getNodeInCut(0)) {
        std::cout << "0 is in cut" << std::endl;
    } else {
        std::cout << "0 is not in cut" << std::endl;
    }
    if(G->getNodeInCut(1)) {
        std::cout << "1 is in cut" << std::endl;
    } else {
        std::cout << "1 is not in cut" << std::endl;
    }

    return edge_cut_size;
}
const std::vector<int>& MinCutCustom::GetInPartition() const {
    return this->in_partition;
}
const std::vector<int>& MinCutCustom::GetOutPartition() const {
    return this->out_partition;
}
