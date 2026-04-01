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
            // Drain into vector for yield analysis
            std::vector<std::pair<std::vector<int>, int>> pending;
            while(!CM::to_be_clustered_clusters.empty()) {
                pending.push_back(CM::to_be_clustered_clusters.front());
                CM::to_be_clustered_clusters.pop();
            }

            std::set<size_t> yield_indices;

            if (!this->yield_dir.empty() && this->yield_node_threshold > 0) {
                // Find sub-clusters large enough to be worth redistributing
                std::vector<size_t> large_indices;
                for (size_t i = 0; i < pending.size(); i++) {
                    if ((int)pending[i].first.size() >= this->yield_node_threshold) {
                        large_indices.push_back(i);
                    }
                }

                // Only yield when there are 2+ large sub-clusters:
                // keep the largest locally, yield the rest. Logic is that the in-flight time could compensate a bit of the processing time
                if (large_indices.size() >= 2) {
                    std::sort(large_indices.begin(), large_indices.end(),
                        [&pending](size_t a, size_t b) {
                            return pending[a].first.size() > pending[b].first.size();
                        });

                    // Keep index 0 (largest). Yield indices 1..N.
                    for (size_t i = 1; i < large_indices.size(); i++) {
                        yield_indices.insert(large_indices[i]);
                    }

                    for (size_t idx : yield_indices) {
                        this->WriteYieldCluster(&graph, pending[idx].first, new_to_originial_id_map);
                    }

                    this->WriteToLogFile("Yielded " + std::to_string(yield_indices.size()) +
                        " sub-clusters, keeping largest (" +
                        std::to_string(pending[large_indices[0]].first.size()) + " nodes) locally",
                        Log::info);
                }
            }

            // Push non-yielded sub-clusters back to local queue
            for (size_t i = 0; i < pending.size(); i++) {
                if (yield_indices.count(i) == 0) {
                    parent_to_child_map[pending[i].second].push_back(next_cluster_id);
                    CM::to_be_mincut_clusters.push({pending[i].first, next_cluster_id});
                    next_cluster_id++;
                }
            }
        }
        iter_count ++;
    }


    this->WriteYieldSummary();

    this->WriteToLogFile("Writing output to: " + this->output_file, Log::info);
    this->WriteClusterQueue(CM::done_being_clustered_clusters, &graph, new_to_originial_id_map);
    this->WriteClusterHistory(parent_to_child_map);
    igraph_destroy(&graph);
    return 0;
}
