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
        std::cerr << "constrained.cpp: finished running leiden-cpm" << std::endl;
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

std::vector<std::vector<int>> GetConnectedComponents(igraph_t* graph_ptr) {
    std::vector<std::vector<int>> connected_components_vector;
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
    for(auto const& [component_id, member_vector] : component_id_to_member_vector_map) {
        connected_components_vector.push_back(member_vector);
    }
    std::cerr << "constrained.cpp: " << number_of_components << " connected components found" << std::endl;
    return connected_components_vector;
}


std::mutex to_be_mincut_mutex;
std::condition_variable to_be_mincut_condition_variable;
std::queue<std::vector<int>> to_be_mincut_clusters;

std::mutex to_be_clustered_mutex;
std::queue<std::vector<int>> to_be_clustered_clusters;

void MincutWorker(igraph_t* graph) {
    while (true) {
        std::cerr << "constrained.cpp: entering while loop for mincut worker" << std::endl;
        std::unique_lock<std::mutex> to_be_mincut_lock{to_be_mincut_mutex};
        to_be_mincut_condition_variable.wait(to_be_mincut_lock, []() {
            return !to_be_mincut_clusters.empty();
        });
        std::vector<int> current_cluster = to_be_mincut_clusters.front();
        to_be_mincut_clusters.pop();
        to_be_mincut_lock.unlock();
        if(current_cluster.size() == 1 && current_cluster[0] == -1) {
            // done with work!
            return;
        }
        igraph_vector_int_t nodes_to_keep;
        igraph_vector_int_t new_id_to_old_id_map;
        igraph_vector_int_init(&new_id_to_old_id_map, current_cluster.size());
        igraph_vector_int_init(&nodes_to_keep, current_cluster.size());
        for(size_t i = 0; i < current_cluster.size(); i ++) {
            VECTOR(nodes_to_keep)[i] = current_cluster[i];
        }
        igraph_t induced_subgraph;
        // technically could just pass in the nodes and edges info directly by iterating through the edges and checking if it's inter vs intracluster
        // likely not too much of a memory or time overhead
        igraph_induced_subgraph_map(graph, &induced_subgraph, igraph_vss_vector(&nodes_to_keep), IGRAPH_SUBGRAPH_CREATE_FROM_SCRATCH, NULL, &new_id_to_old_id_map);
        MinCutCustom mcc(&induced_subgraph);
        int edge_cut_size = mcc.ComputeMinCut();
        std::cerr << "constrained.cpp: some thread got the lock and obtained a cluster to be mincut. cluster size: " << current_cluster.size() << " and mincut edge size: " << edge_cut_size << std::endl;
        if(edge_cut_size != 0 && edge_cut_size < log10(current_cluster.size())) {
            std::vector<int> in_partition = mcc.GetInPartition();
            std::vector<int> out_partition = mcc.GetOutPartition();
            for(size_t i = 0; i < in_partition.size(); i ++) {
                in_partition[i] = VECTOR(new_id_to_old_id_map)[in_partition[i]];
            }
            for(size_t i = 0; i < out_partition.size(); i ++) {
                out_partition[i] = VECTOR(new_id_to_old_id_map)[out_partition[i]];
            }
            std::unique_lock<std::mutex> to_be_clustered_lock{to_be_clustered_mutex};
            to_be_clustered_clusters.push(in_partition);
            to_be_clustered_clusters.push(out_partition);
            to_be_clustered_lock.unlock();
        } else {
            std::unique_lock<std::mutex> to_be_clustered_lock{to_be_clustered_mutex};
            to_be_clustered_clusters.push(current_cluster);
            to_be_clustered_lock.unlock();
        }

        igraph_destroy(&induced_subgraph);
    }
}

void ConstrainedClustering::RemoveInterClusterEdges(igraph_t* graph, const std::map<int,int>& node_id_to_cluster_id_map) {
    igraph_vector_int_t edges_to_remove;
    igraph_vector_int_init(&edges_to_remove, 0);
    igraph_eit_t eit;
    igraph_eit_create(graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
    for(; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
        igraph_integer_t current_edge = IGRAPH_EIT_GET(eit);
        int from_node = IGRAPH_FROM(graph, current_edge);
        int to_node = IGRAPH_TO(graph, current_edge);
        if(node_id_to_cluster_id_map.at(from_node) != node_id_to_cluster_id_map.at(to_node)) {
            igraph_vector_int_push_back(&edges_to_remove, IGRAPH_EIT_GET(eit));
        }
    }
    igraph_es_t es;
    igraph_es_vector_copy(&es, &edges_to_remove);
    igraph_delete_edges(graph, es);
    igraph_eit_destroy(&eit);
    igraph_es_destroy(&es);
    igraph_vector_int_destroy(&edges_to_remove);
}

int MinCutGlobalClusterRepeat::main() {
    // load edgelist into igraph
    this->write_to_log_file("Loading the initial graph" , 1);
    FILE* edgelist_file = fopen(this->edgelist.c_str(), "r");
    igraph_t graph;
    igraph_read_graph_edgelist(&graph, edgelist_file, 0, false);
    fclose(edgelist_file);
    this->write_to_log_file("Finished loading the initial graph" , 1);


    int before_mincut_number_of_clusters = -1;
    int after_mincut_number_of_clusters = -2;
    // get its connected components
    while (true) {
        std::vector<std::vector<int>> connected_components_vector = GetConnectedComponents(&graph);
        for(size_t i = 0; i < connected_components_vector.size(); i ++) {
            to_be_mincut_clusters.push(connected_components_vector[i]);
        }
        std::cerr << "constrained.cpp: " << to_be_mincut_clusters.size() << " number of clusters to be mincut" << std::endl;
        before_mincut_number_of_clusters = to_be_mincut_clusters.size();
        for(int i = 0; i < this->num_processors; i ++) {
            to_be_mincut_clusters.push({-1});
        }
        std::vector<std::thread> thread_vector;
        for(int i = 0; i < this->num_processors; i ++) {
            thread_vector.push_back(std::thread(MincutWorker, &graph));
        }
        for(size_t thread_index = 0; thread_index < thread_vector.size(); thread_index ++) {
            thread_vector[thread_index].join();
        }
        std::cerr << "constrained.cpp: " << to_be_clustered_clusters.size() << " number of clusters to be clustered" << std::endl;
        after_mincut_number_of_clusters = to_be_clustered_clusters.size();
        if(before_mincut_number_of_clusters == after_mincut_number_of_clusters) {
            std::cerr << "constrained.cpp: all clusters are well-connected" << std::endl;
            break;
        }


        std::map<int, int> node_id_to_cluster_id_map;
        int cluster_id = 0;
        while(!to_be_clustered_clusters.empty()) {
            std::vector<int> current_cluster = to_be_clustered_clusters.front();
            to_be_clustered_clusters.pop();
            for(size_t i = 0; i < current_cluster.size(); i ++) {
                node_id_to_cluster_id_map[current_cluster[i]] = cluster_id;
            }
            cluster_id ++;
        }
        this->RemoveInterClusterEdges(&graph, node_id_to_cluster_id_map);
        int seed = 0;
        node_id_to_cluster_id_map = get_communities("", this->algorithm, seed, this->resolution, &graph);
        this->RemoveInterClusterEdges(&graph, node_id_to_cluster_id_map);
    }


    igraph_destroy(&graph);

    std::cerr << "constrained.cpp: final clusters:" << std::endl;
    while(!to_be_clustered_clusters.empty()) {
        std::vector<int> current_cluster = to_be_clustered_clusters.front();
        to_be_clustered_clusters.pop();
        std::cerr << "constrained.cpp: new cluster" << std::endl;;
        std::cerr << "constrained.cpp: ";
        for(size_t i = 0; i < current_cluster.size(); i ++) {
            std::cerr << current_cluster[i] << " ";
        }
        std::cerr << std::endl;
    }
    return 0;
}
