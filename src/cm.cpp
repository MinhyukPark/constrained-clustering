#include "cm.h"

int CM::main() {
    this->WriteToLogFile("Loading the initial graph" , Log::info);
    std::map<std::string, int> original_to_new_id_map = this->GetOriginalToNewIdMap(this->edgelist);
    std::map<int, std::string> new_to_originial_id_map = this->InvertMap(original_to_new_id_map);
    igraph_t graph;
    /* igraph_set_attribute_table(&igraph_cattribute_table); */
    igraph_empty(&graph, 0, IGRAPH_UNDIRECTED);

    this->LoadEdgesFromFile(&graph, this->edgelist, original_to_new_id_map);
    this->WriteToLogFile("Finished loading the initial graph" , Log::info);

    int after_mincut_number_of_clusters = -2;
    int iter_count = 0;
    std::map<int, int> node_id_to_cluster_id_map;
    if(this->existing_clustering == "") {
        int seed = 0;
        node_id_to_cluster_id_map = ConstrainedClustering::GetCommunities("", this->algorithm, seed, this->clustering_parameter, &graph);
        ConstrainedClustering::RemoveInterClusterEdges(&graph, node_id_to_cluster_id_map);
    } else if(this->existing_clustering != "") {
        node_id_to_cluster_id_map = ConstrainedClustering::ReadCommunities(original_to_new_id_map, this->existing_clustering);
        ConstrainedClustering::RemoveInterClusterEdges(&graph, node_id_to_cluster_id_map);
    }
    std::map<int, std::vector<int>> cluster_id_to_node_id_map = ConstrainedClustering::ReverseMap(node_id_to_cluster_id_map); // graph node id
    int next_cluster_id = ConstrainedClustering::FindMaxClusterId(cluster_id_to_node_id_map) + 1;

    std::map<int, std::vector<int>> parent_to_child_map;
    std::vector<std::vector<int>> connected_components_vector = ConstrainedClustering::GetConnectedComponents(&graph);
    for(size_t i = 0; i < connected_components_vector.size(); i ++) {
        int parent_cluster_id = -1; // -1 indicates root
        int current_cluster_id = -1;

        int first_node_in_subcluster = connected_components_vector[i][0];
        int original_cluster_id_of_first_node_in_subcluster = node_id_to_cluster_id_map.at(first_node_in_subcluster);
        int size_of_original_cluster = cluster_id_to_node_id_map[original_cluster_id_of_first_node_in_subcluster].size();
        int size_of_sub_cluster = connected_components_vector[i].size();
        if (size_of_original_cluster == size_of_sub_cluster) {
            current_cluster_id = original_cluster_id_of_first_node_in_subcluster;
        } else {
            bool original_cluster_id_of_first_node_in_subcluster_found = false;
            for (size_t j = 0; j < parent_to_child_map[-1].size(); j ++) {
                if (parent_to_child_map[-1][j] == original_cluster_id_of_first_node_in_subcluster) {
                    original_cluster_id_of_first_node_in_subcluster_found = true;
                    break;
                }
            }
            if (!original_cluster_id_of_first_node_in_subcluster_found) {
                parent_to_child_map[-1].push_back(original_cluster_id_of_first_node_in_subcluster);
            }
            parent_cluster_id = original_cluster_id_of_first_node_in_subcluster;
            current_cluster_id = next_cluster_id;
            next_cluster_id ++;
        }
        parent_to_child_map[parent_cluster_id].push_back(current_cluster_id);
        CM::to_be_mincut_clusters.push({connected_components_vector[i], current_cluster_id});
    }
    int previous_done_being_clustered_size = 0;
    while (true) {
        this->WriteToLogFile("Iteration number: " + std::to_string(iter_count), Log::debug);
        if(iter_count % 10 == 0) {
            this->WriteToLogFile("Iteration number: " + std::to_string(iter_count), Log::info);
        }
        this->WriteToLogFile(std::to_string(CM::to_be_mincut_clusters.size()) + " [connected components / clusters] to be mincut", Log::debug);
        /* if a thread gets a cluster {-1}, then they know processing is done and they can stop working */
        for(int i = 0; i < this->num_processors; i ++) {
            CM::to_be_mincut_clusters.push({{-1}, -1});
        }
        std::vector<std::thread> thread_vector;
        for(int i = 0; i < this->num_processors; i ++) {
            int seed = 0;
            thread_vector.push_back(std::thread(CM::MinCutOrClusterWorker, &graph, this->algorithm, seed, this->clustering_parameter, this->current_connectedness_criterion, this->connectedness_criterion_c, this->connectedness_criterion_x, this->pre_computed_log, this->prune, this->mincut_type));
        }
        /* the results from each thread gets stored in to_be_clustered_clusters */
        for(size_t thread_index = 0; thread_index < thread_vector.size(); thread_index ++) {
            thread_vector[thread_index].join();
        }
        this->WriteToLogFile(std::to_string(CM::to_be_clustered_clusters.size()) + " [connected components / clusters] to be clustered after a round of mincuts", Log::debug);
        this->WriteToLogFile(std::to_string(CM::done_being_clustered_clusters.size() - previous_done_being_clustered_size) + " [connected components / clusters] were found to be well connected", Log::debug);
        previous_done_being_clustered_size = CM::done_being_clustered_clusters.size();
        /* so to_be_clustered_clusters is a list of remaining work */
        after_mincut_number_of_clusters = CM::to_be_clustered_clusters.size();
        if(after_mincut_number_of_clusters == 0) {
            this->WriteToLogFile("all clusters are well-connected", Log::info);
            this->WriteToLogFile("Total number of iterations: " + std::to_string(iter_count + 1), Log::info);
            break;
        } else {
            while(!CM::to_be_clustered_clusters.empty()) {
                std::pair<std::vector<int>, int> current_front = CM::to_be_clustered_clusters.front(); // second here is the parent cluster id
                parent_to_child_map[current_front.second].push_back(next_cluster_id);
                CM::to_be_mincut_clusters.push({current_front.first, next_cluster_id});
                next_cluster_id ++;
                CM::to_be_clustered_clusters.pop();
            }
        }
        iter_count ++;
    }


    this->WriteToLogFile("Writing output to: " + this->output_file, Log::info);
    this->WriteClusterQueue(CM::done_being_clustered_clusters, &graph, new_to_originial_id_map);
    this->WriteClusterHistory(parent_to_child_map);
    igraph_destroy(&graph);
    return 0;
}
