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

    if(this->existing_clustering == "") {
        int seed = 0;
        std::map<int, int> node_id_to_cluster_id_map = ConstrainedClustering::GetCommunities("", this->algorithm, seed, this->clustering_parameter, &graph);
        ConstrainedClustering::RemoveInterClusterEdges(&graph, node_id_to_cluster_id_map);
    } else if(this->existing_clustering != "") {
        std::map<int, int> new_node_id_to_cluster_id_map = ConstrainedClustering::ReadCommunities(original_to_new_id_map, this->existing_clustering);
        ConstrainedClustering::RemoveInterClusterEdges(&graph, new_node_id_to_cluster_id_map);
    }

    std::vector<std::vector<int>> connected_components_vector = ConstrainedClustering::GetConnectedComponents(&graph);
    for(size_t i = 0; i < connected_components_vector.size(); i ++) {
        CM::to_be_mincut_clusters.push(connected_components_vector[i]);
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
            CM::to_be_mincut_clusters.push({-1});
        }
        std::vector<std::thread> thread_vector;
        for(int i = 0; i < this->num_processors; i ++) {
            int seed = 0;
            thread_vector.push_back(std::thread(CM::MinCutOrClusterWorker, &graph, this->algorithm, seed, this->clustering_parameter, this->current_connectedness_criterion, this->connectedness_criterion_c, this->connectedness_criterion_x, this->pre_computed_log, this->mincut_type));
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
                CM::to_be_mincut_clusters.push(CM::to_be_clustered_clusters.front());
                CM::to_be_clustered_clusters.pop();
            }
        }
        iter_count ++;
    }


    this->WriteToLogFile("Writing output to: " + this->output_file, Log::info);
    this->WriteClusterQueue(CM::done_being_clustered_clusters, &graph, new_to_originial_id_map);
    igraph_destroy(&graph);
    return 0;
}
