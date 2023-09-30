#include "mincut_custom.h"

int MinCutCustom::ComputeMinCut() {
    int edge_cut_size = 0;
    auto cfg = configuration::getConfig();
    cfg->algorithm = "cactus";
    cfg->queue_type = "bqueue";
    cfg->find_most_balanced_cut = true;
    cfg->threads = 1;
    cfg->save_cut = true;
    cfg->set_node_in_cut = true;
    random_functions::setSeed(0);
    std::shared_ptr<graph_type> G = std::make_shared<graph_type>();

    // make input output mapping of node ids
    // make sure graph is connected
    // igraph maybe just always has increasing node ids starting at 0 and also is connected
    int num_nodes = igraph_vcount(this->graph);
    int num_edges = igraph_ecount(this->graph);
    std::cerr << "mincut_custom.cpp: mincut being requested on " << num_nodes << " nodes and " << num_edges << " edges" << std::endl;
    G->start_construction(num_nodes, num_edges * 2);
    for(int i = 0; i < num_nodes; i ++) {
        NodeID current_node = G->new_node();
    }

    igraph_eit_t eit;
    igraph_eit_create(this->graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
    for (; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
        igraph_integer_t current_edge = IGRAPH_EIT_GET(eit);
        int from_node = IGRAPH_FROM(this->graph, current_edge);
        int to_node = IGRAPH_TO(this->graph, current_edge);
        std::cerr << "mincut_custom.cpp: edge from " << from_node << " to " << to_node << std::endl;
        G->new_edge(from_node, to_node, 1);
        G->new_edge(to_node, from_node, 1);
    }
    igraph_eit_destroy(&eit);

    G->finish_construction();

    /* auto* cmc = selectMincutAlgorithm<GraphPtr>(cfg->algorithm); */
    auto* cmc = new cactus_mincut<GraphPtr>();
    EdgeWeight cut = cmc->perform_minimum_cut(G);
    std::cerr << "mincut_custom.cpp: current edge cut size is " << cut << std::endl;
    edge_cut_size = cut;


    for(int node_id = 0; node_id < num_nodes; node_id ++) {
        std::cerr << "mincut_custom.cpp: node cut info for node_id " << node_id << " :: " << G->getNodeInCut(node_id) << std::endl;
        if(G->getNodeInCut(node_id)) {
            std::cerr << "mincut_custom.cpp: " << node_id << " is in cut" << std::endl;
            this->in_partition.push_back(node_id);
        } else {
            std::cerr << "mincut_custom.cpp: " << node_id << " is not in cut" << std::endl;
            this->out_partition.push_back(node_id);
        }
    }

    return edge_cut_size;
}
const std::vector<int>& MinCutCustom::GetInPartition() const {
    return this->in_partition;
}
const std::vector<int>& MinCutCustom::GetOutPartition() const {
    return this->out_partition;
}
