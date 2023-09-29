#include "constrained.h"

void ConstrainedClustering::write_partition_map(std::map<int, int>& final_partition) {
    std::ofstream clustering_output(this->output_file);
    for(auto const& [node_id, cluster_id]: final_partition) {
        clustering_output << node_id << " " << cluster_id << '\n';
    }
    clustering_output.close();
}

void run_louvain_and_update_partition(std::map<int, int>& partition_map, int seed, igraph_t* graph) {
    igraph_vector_int_t membership;
    igraph_vector_int_init(&membership, 0);
    igraph_rng_seed(igraph_rng_default(), seed);
    igraph_community_multilevel(graph, 0, 1, &membership, 0, 0);

    igraph_eit_t eit;
    igraph_eit_create(graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
    std::set<int> visited;
    for (; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
        igraph_integer_t current_edge = IGRAPH_EIT_GET(eit);
        int from_node = IGRAPH_FROM(graph, current_edge);
        int to_node = IGRAPH_TO(graph, current_edge);
        if(!visited.contains(from_node)) {
            visited.insert(from_node);
            partition_map[from_node] = VECTOR(membership)[from_node];
        }
        if(!visited.contains(to_node)) {
            visited.insert(to_node);
            partition_map[to_node] = VECTOR(membership)[to_node];
        }
    }
    igraph_eit_destroy(&eit);
    igraph_vector_int_destroy(&membership);
}

void run_leiden_and_update_partition(std::map<int, int>& partition_map, MutableVertexPartition* partition, igraph_t* graph) {
    Optimiser o;
    o.optimise_partition(partition);
    igraph_eit_t eit;
    igraph_eit_create(graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
    std::set<int> visited;
    for (; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
        igraph_integer_t current_edge = IGRAPH_EIT_GET(eit);
        int from_node = IGRAPH_FROM(graph, current_edge);
        int to_node = IGRAPH_TO(graph, current_edge);
        if(!visited.contains(from_node)) {
            visited.insert(from_node);
            partition_map[from_node] = partition->membership(from_node);
        }
        if(!visited.contains(to_node)) {
            visited.insert(to_node);
            partition_map[to_node] = partition->membership(to_node);
        }
    }
    igraph_eit_destroy(&eit);
}

void set_igraph_all_edges_weight(igraph_t* graph, double weight) {
    igraph_eit_t eit;
    igraph_eit_create(graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
    for(; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
        SETEAN(graph, "weight", IGRAPH_EIT_GET(eit), 1);
    }
    igraph_eit_destroy(&eit);
}

std::map<int, int> get_communities(std::string edgelist, std::string algorithm, int seed, double clustering_parameter, igraph_t* graph_ptr) {
    std::map<int, int> partition_map;
    igraph_t graph;
    if(graph_ptr == NULL) {
        FILE* edgelist_file = fopen(edgelist.c_str(), "r");
        igraph_set_attribute_table(&igraph_cattribute_table);
        igraph_read_graph_edgelist(&graph, edgelist_file, 0, false);
        fclose(edgelist_file);
        set_igraph_all_edges_weight(&graph, 1);

    } else {
        graph = *graph_ptr;
    }

    if(algorithm == "louvain") {
        run_louvain_and_update_partition(partition_map, seed, &graph);
    } else if(algorithm == "leiden-cpm") {
        Graph leiden_graph(&graph);
        CPMVertexPartition partition(&leiden_graph, clustering_parameter);
        run_leiden_and_update_partition(partition_map, &partition, &graph);
    } else if(algorithm == "leiden-mod") {
        Graph leiden_graph(&graph);
        ModularityVertexPartition partition(&leiden_graph);
        run_leiden_and_update_partition(partition_map, &partition, &graph);
    } else {
        throw std::invalid_argument("get_communities(): Unsupported algorithm");
    }

    if(graph_ptr == NULL) {
        igraph_destroy(&graph);
    }
    return partition_map;
}

/*
 * message type here is 1 for INFO, 2 for DEBUG, and -1 for ERROR
 */
int ConstrainedClustering::write_to_log_file(std::string message, int message_type) {
    if(this->log_level > 0) {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        std::string log_message_prefix;
        if(message_type == 1) {
            log_message_prefix = "[INFO]";
        } else if(message_type == 2) {
            log_message_prefix = "[DEBUG]";
        } else if(message_type == -1) {
            log_message_prefix = "[ERROR]";
        }
        auto days_elapsed = std::chrono::duration_cast<std::chrono::days>(now - this->start_time);
        auto hours_elapsed = std::chrono::duration_cast<std::chrono::hours>(now - this->start_time - days_elapsed);
        auto minutes_elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - this->start_time - days_elapsed - hours_elapsed);
        auto seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - this->start_time - days_elapsed - hours_elapsed - minutes_elapsed);
        auto total_seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - this->start_time);
        log_message_prefix += "[";
        log_message_prefix += std::to_string(days_elapsed.count());
        log_message_prefix += "-";
        log_message_prefix += std::to_string(hours_elapsed.count());
        log_message_prefix += ":";
        log_message_prefix += std::to_string(minutes_elapsed.count());
        log_message_prefix += ":";
        log_message_prefix += std::to_string(seconds_elapsed.count());
        log_message_prefix += "]";

        log_message_prefix += "(t=";
        log_message_prefix += std::to_string(total_seconds_elapsed.count());
        log_message_prefix += "s)";
        this->log_file_handle << log_message_prefix << " " << message << '\n';
    }
    return 0;
}

std::map<int, std::vector<int>> GetConnectedComponents(igraph_t* graph_ptr) {
    std::map<int, std::vector<int>> component_id_to_member_vector_map;
    igraph_vector_int_t component_id_vector;
    igraph_vector_int_init(&component_id_vector, 0);
    igraph_vector_int_t membership_size_vector;
    igraph_vector_int_init(&membership_size_vector, 0);
    igraph_integer_t number_of_components;
    igraph_connected_components(graph_ptr, &component_id_vector, &membership_size_vector, &number_of_components, IGRAPH_WEAK);
    for(int node_id = 0; node_id < igraph_vcount(graph_ptr); node_id ++) {
        int current_component_id = VECTOR(component_id_vector)[node_id];
        if(VECTOR(membership_size_vector)[current_component_id] > 1) {
            component_id_to_member_vector_map[current_component_id].push_back(node_id);
        }
    }
    igraph_vector_int_destroy(&component_id_vector);
    igraph_vector_int_destroy(&membership_size_vector);
    return component_id_to_member_vector_map;
}

int MinCutGlobalClusterRepeat::main() {
    std::cerr << "got to mcgcr main" << std::endl;
    // load edgelist into igraph
    this->write_to_log_file("Loading the initial graph" , 1);
    FILE* edgelist_file = fopen(this->edgelist.c_str(), "r");
    igraph_t graph;
    igraph_read_graph_edgelist(&graph, edgelist_file, 0, false);
    fclose(edgelist_file);
    this->write_to_log_file("Finished loading the initial graph" , 1);

    std::cerr << "loaded graph" << std::endl;

    std::queue<std::vector<int>> to_be_mincut_clusters;

    // get its connected components
    std::map<int, std::vector<int>> component_id_to_member_vector_map = GetConnectedComponents(&graph);
    std::cerr << "got connected components" << std::endl;
    for(auto const& [component_id, member_vector] : component_id_to_member_vector_map) {
        std::cout << "component_id: " << component_id << " - ";
        for(int i = 0; i < member_vector.size(); i ++) {
            std::cout << member_vector[i] << " ";
        }
        std::cout << std::endl;
        to_be_mincut_clusters.push(member_vector);
    }


    // for every component with size greater than zero,
    // add to input queue the node list with respect to the global graph
    //
    //
    // each worker:
    // pop from queue
    // make induced subgraph on the nodelist
    // run mincut on it
    // push the two partitions (or 1 if well-connected condition is satified) to output queue with node ids with respect to the global graph
    //
    // make map of node id to cluster id here (partition id)
    // iterate through edges of the global graph
    // add to remove list if two ends of the edge are not in the same partition
    // remove edges
    // run leiden
    // this is the saved clustering
    // push each clustering as node list into queue for mincut workers
    // check if the output queue clustering is the same as the saved clustering, otherwise continue


    return 0;
}
