#include "mincut_custom.h"

int MinCutCustom::ComputeMinCut() {
    igraph_vector_int_t node_degrees;
    igraph_vector_int_init(&node_degrees, igraph_vcount(this->graph));
    igraph_degree(this->graph, &node_degrees, igraph_vss_all(), IGRAPH_ALL, IGRAPH_NO_LOOPS);
    int one_degree_node = -1;
    for(int i = 0; i < igraph_vcount(this->graph); i ++) {
        if(VECTOR(node_degrees)[i] == 1) {
            one_degree_node = i;
            break;
        }
    }
    if(one_degree_node != -1) {
        /* std::cerr<< "one degree node found" <<std::endl; */
        this->in_partition.push_back(one_degree_node);
        for(int i = 0; i < igraph_vcount(this->graph); i ++) {
            if(i != one_degree_node) {
                this->out_partition.push_back(i);
            }
        }
        return 1;
    }

    int edge_cut_size = -1;
    auto cfg = configuration::getConfig();
    cfg->find_most_balanced_cut = true;
    cfg->threads = 1;
    cfg->save_cut = true;
    cfg->set_node_in_cut = true;
    // uncomment this if you want random seeds
    // comment this out if you want a set seed
    // it's unintuitive but trust me
    /* random_functions::setSeed(0); */
    std::shared_ptr<mutable_graph> G = std::make_shared<mutable_graph>();

    // make input output mapping of node ids
    // make sure graph is connected
    // igraph maybe just always has increasing node ids starting at 0 and also is connected
    int num_nodes = igraph_vcount(this->graph);
    G->start_construction(num_nodes);
    for(int i = 0; i < num_nodes; i ++) {
        NodeID current_node = G->new_node();
        G->setPartitionIndex(current_node, 0);
    }

    igraph_eit_t eit;
    igraph_eit_create(this->graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
    for (; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
        igraph_integer_t current_edge = IGRAPH_EIT_GET(eit);
        int from_node = IGRAPH_FROM(this->graph, current_edge);
        int to_node = IGRAPH_TO(this->graph, current_edge);
        /* std::cerr << "mincut_custom.cpp: edge from " << from_node << " to " << to_node << std::endl; */
        int source_node = -1;
        int target_node = -1;
        if(from_node < target_node) {
            source_node = from_node;
            target_node = to_node;
        } else {
            source_node = to_node;
            target_node = from_node;
        }
        /* if(igraph_cattribute_has_attr(this->graph, IGRAPH_ATTRIBUTE_EDGE, "weight")) { */
            /* G->new_edge(source_node, target_node, EAN(this->graph, "weight", current_edge)); */
        /* } else { */
        G->new_edge(source_node, target_node, 1);
        /* G->new_edge(source_node, target_node, 0); */
        /* } */
    }
    igraph_eit_destroy(&eit);

    G->finish_construction();
    G->computeDegrees();

    cactus_mincut<std::shared_ptr<mutable_graph>> cmc;
    edge_cut_size = cmc.perform_minimum_cut(G);
    /* std::cerr << "mincut_custom.cpp: current edge cut size is " << edge_cut_size << std::endl; */


    for(int node_id = 0; node_id < num_nodes; node_id ++) {
        /* std::cerr << "mincut_custom.cpp: node cut info for node_id " << node_id << " :: " << G->getNodeInCut(node_id) << std::endl; */
        if(G->getNodeInCut(node_id)) {
            /* std::cerr << "mincut_custom.cpp: " << node_id << " is in cut" << std::endl; */
            this->in_partition.push_back(node_id);
        } else {
            /* std::cerr << "mincut_custom.cpp: " << node_id << " is not in cut" << std::endl; */
            this->out_partition.push_back(node_id);
        }
    }

    return edge_cut_size;
}

/* int MinCutCustom::ComputeAllMinCuts() { */
/*     int edge_cut_size = -1; */
/*     auto cfg = configuration::getConfig(); */
/*     cfg->find_most_balanced_cut = true; */
/*     cfg->threads = 1; */
/*     cfg->save_cut = true; */
/*     cfg->set_node_in_cut = true; */
/*     // uncomment this if you want random seeds */
/*     // comment this out if you want a set seed */
/*     // it's unintuitive but trust me */
/*     /1* random_functions::setSeed(0); *1/ */
/*     std::shared_ptr<mutable_graph> G = std::make_shared<mutable_graph>(); */

/*     // make input output mapping of node ids */
/*     // make sure graph is connected */
/*     // igraph maybe just always has increasing node ids starting at 0 and also is connected */
/*     int num_nodes = igraph_vcount(this->graph); */
/*     G->start_construction(num_nodes); */
/*     for(int i = 0; i < num_nodes; i ++) { */
/*         NodeID current_node = G->new_node(); */
/*         G->setPartitionIndex(current_node, 0); */
/*     } */

/*     igraph_eit_t eit; */
/*     igraph_eit_create(this->graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit); */
/*     for (; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) { */
/*         igraph_integer_t current_edge = IGRAPH_EIT_GET(eit); */
/*         int from_node = IGRAPH_FROM(this->graph, current_edge); */
/*         int to_node = IGRAPH_TO(this->graph, current_edge); */
/*         /1* std::cerr << "mincut_custom.cpp: edge from " << from_node << " to " << to_node << std::endl; *1/ */
/*         int source_node = -1; */
/*         int target_node = -1; */
/*         if(from_node < target_node) { */
/*             source_node = from_node; */
/*             target_node = to_node; */
/*         } else { */
/*             source_node = to_node; */
/*             target_node = from_node; */
/*         } */
/*         /1* if(igraph_cattribute_has_attr(this->graph, IGRAPH_ATTRIBUTE_EDGE, "weight")) { *1/ */
/*             /1* G->new_edge(source_node, target_node, EAN(this->graph, "weight", current_edge)); *1/ */
/*         /1* } else { *1/ */
/*         G->new_edge(source_node, target_node, 1); */
/*         /1* G->new_edge(source_node, target_node, 0); *1/ */
/*         /1* } *1/ */
/*     } */
/*     igraph_eit_destroy(&eit); */

/*     G->finish_construction(); */
/*     G->computeDegrees(); */

/*     cactus_mincut<std::shared_ptr<mutable_graph>> cmc; */
/*     edge_cut_size = cmc.perform_minimum_cut(G); */
/*     /1* std::cerr << "mincut_custom.cpp: current edge cut size is " << edge_cut_size << std::endl; *1/ */


/*     for(int node_id = 0; node_id < num_nodes; node_id ++) { */
/*         /1* std::cerr << "mincut_custom.cpp: node cut info for node_id " << node_id << " :: " << G->getNodeInCut(node_id) << std::endl; *1/ */
/*         if(G->getNodeInCut(node_id)) { */
/*             /1* std::cerr << "mincut_custom.cpp: " << node_id << " is in cut" << std::endl; *1/ */
/*             this->in_partition.push_back(node_id); */
/*         } else { */
/*             /1* std::cerr << "mincut_custom.cpp: " << node_id << " is not in cut" << std::endl; *1/ */
/*             this->out_partition.push_back(node_id); */
/*         } */
/*     } */

/*     return edge_cut_size; */
/* } */
const std::vector<int>& MinCutCustom::GetInPartition() const {
    return this->in_partition;
}
const std::vector<int>& MinCutCustom::GetOutPartition() const {
    return this->out_partition;
}
/* const std::vector<int>& MinCutCustom::GetAllPartitions() const { */
/*     return this->all_partitions; */
/* } */
