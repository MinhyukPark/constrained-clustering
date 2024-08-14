#ifndef CM_H
#define CM_H
#include "constrained.h"


class CM : public ConstrainedClustering {
    public:
        CM(std::string edgelist, std::string algorithm, double clustering_parameter, std::string existing_clustering, int num_processors, std::string output_file, std::string log_file, int log_level) : ConstrainedClustering(edgelist, algorithm, clustering_parameter, existing_clustering, num_processors, output_file, log_file, log_level) {
        };
        int main() override;

        static inline std::vector<std::vector<int>> RunClusterOnPartition(const igraph_t* graph, std::string algorithm, int seed, double clustering_parameter, std::vector<int>& partition) {
            std::vector<std::vector<int>> cluster_vectors;
            std::map<int, std::vector<int>> cluster_map;
            igraph_vector_int_t nodes_to_keep;
            igraph_vector_int_t new_id_to_old_id_map;
            igraph_vector_int_init(&new_id_to_old_id_map, partition.size());
            igraph_vector_int_init(&nodes_to_keep, partition.size());
            for(size_t i = 0; i < partition.size(); i ++) {
                VECTOR(nodes_to_keep)[i] = partition[i];
            }
            igraph_t induced_subgraph;
            igraph_induced_subgraph_map(graph, &induced_subgraph, igraph_vss_vector(&nodes_to_keep), IGRAPH_SUBGRAPH_CREATE_FROM_SCRATCH, NULL, &new_id_to_old_id_map);
            std::map<int, int> partition_map  = ConstrainedClustering::GetCommunities("", algorithm, seed, clustering_parameter, &induced_subgraph);
            ConstrainedClustering::RemoveInterClusterEdges(&induced_subgraph, partition_map);
            std::vector<std::vector<int>> connected_components_vector = ConstrainedClustering::GetConnectedComponents(&induced_subgraph);
            for(size_t i = 0; i < connected_components_vector.size(); i ++) {
                std::vector<int> translated_cluster_vector;
                for(size_t j = 0; j < connected_components_vector.at(i).size(); j ++) {
                    int new_id = connected_components_vector.at(i).at(j);
                    translated_cluster_vector.push_back(VECTOR(new_id_to_old_id_map)[new_id]);
                }
                cluster_vectors.push_back(translated_cluster_vector);
            }
            igraph_vector_int_destroy(&nodes_to_keep);
            igraph_vector_int_destroy(&new_id_to_old_id_map);
            igraph_destroy(&induced_subgraph);
            return cluster_vectors;
        }

        static inline void MinCutOrClusterWorker(const igraph_t* graph, std::string algorithm, int seed, double clustering_parameter) {
            while (true) {
                std::unique_lock<std::mutex> to_be_mincut_lock{to_be_mincut_mutex};
                std::vector<int> current_cluster = CM::to_be_mincut_clusters.front();
                std::set<int> current_cluster_set(current_cluster.begin(), current_cluster.end());
                CM::to_be_mincut_clusters.pop();
                to_be_mincut_lock.unlock();
                if(current_cluster.size() == 1 && current_cluster[0] == -1) {
                    // done with work!
                    return;
                }
                igraph_vector_int_t nodes_to_keep;
                igraph_vector_int_t new_id_to_old_id_vector_map;
                igraph_vector_int_init(&nodes_to_keep, current_cluster.size());
                for(size_t i = 0; i < current_cluster.size(); i ++) {
                    VECTOR(nodes_to_keep)[i] = current_cluster[i];
                }
                std::vector<int> in_partition;
                std::vector<int> out_partition;
                bool is_well_connected = false;
                bool is_non_trivial_cut = false;
                int edge_cut_size = -1;
                igraph_t induced_subgraph;
                int num_nodes_removed = 0;
                // we do mincuts until it's a non trivial mincut or if the current cluster is already well connected (perhaps after some trivial mincuts)
                igraph_vector_int_init(&new_id_to_old_id_vector_map, igraph_vector_int_size(&nodes_to_keep));
                igraph_induced_subgraph_map(graph, &induced_subgraph, igraph_vss_vector(&nodes_to_keep), IGRAPH_SUBGRAPH_CREATE_FROM_SCRATCH, NULL, &new_id_to_old_id_vector_map);
                std::map<int, int> new_id_to_old_id_map;
                std::map<int, int> newnew_id_to_old_id_map;
                std::map<int, int> old_id_to_new_id_map;
                for(int i = 0; i < igraph_vector_int_size(&new_id_to_old_id_vector_map); i ++) {
                    new_id_to_old_id_map[i] = VECTOR(new_id_to_old_id_vector_map)[i];
                    old_id_to_new_id_map[VECTOR(new_id_to_old_id_vector_map)[i]] = i;
                }
                while(!(is_non_trivial_cut || is_well_connected)) {
                    MinCutCustom mcc(&induced_subgraph);
                    edge_cut_size = mcc.ComputeMinCut();
                    std::vector<int> in_partition_candidate = mcc.GetInPartition();
                    std::vector<int> out_partition_candidate = mcc.GetOutPartition();
                    is_well_connected = ConstrainedClustering::IsWellConnected(in_partition_candidate, out_partition_candidate, edge_cut_size, &induced_subgraph);
                    is_non_trivial_cut = in_partition_candidate.size() > 1 && out_partition_candidate.size() > 1;
                    if(is_well_connected || is_non_trivial_cut) {
                        in_partition = in_partition_candidate;
                        out_partition = out_partition_candidate;
                    } else {
                        int node_to_remove = -1;
                        if(in_partition_candidate.size() == 1) {
                            igraph_vector_int_t newnew_id_to_new_id_map;
                            igraph_vector_int_init(&newnew_id_to_new_id_map, out_partition_candidate.size());
                            node_to_remove = in_partition_candidate.at(0);
                            current_cluster_set.erase(new_id_to_old_id_map[node_to_remove]);
                            igraph_delete_vertices_idx(&induced_subgraph, igraph_vss_1(node_to_remove), NULL, &newnew_id_to_new_id_map);
                            for(int i = 0; i < igraph_vector_int_size(&newnew_id_to_new_id_map); i ++) {
                                int newnew_id = i;
                                int new_id = VECTOR(newnew_id_to_new_id_map)[newnew_id];
                                int old_id = new_id_to_old_id_map[new_id];
                                old_id_to_new_id_map[old_id] = newnew_id;
                                newnew_id_to_old_id_map[newnew_id] = old_id;
                            }
                            new_id_to_old_id_map = newnew_id_to_old_id_map;
                            igraph_vector_int_destroy(&newnew_id_to_new_id_map);
                            num_nodes_removed ++;
                        }
                        if(out_partition_candidate.size() == 1) {
                            igraph_vector_int_t newnew_id_to_new_id_map;
                            igraph_vector_int_init(&newnew_id_to_new_id_map, out_partition_candidate.size());
                            node_to_remove = out_partition_candidate.at(0);
                            current_cluster_set.erase(new_id_to_old_id_map[node_to_remove]);
                            igraph_delete_vertices_idx(&induced_subgraph, igraph_vss_1(node_to_remove), NULL, &newnew_id_to_new_id_map);
                            for(int i = 0; i < igraph_vector_int_size(&newnew_id_to_new_id_map); i ++) {
                                int newnew_id = i;
                                int new_id = VECTOR(newnew_id_to_new_id_map)[newnew_id];
                                int old_id = new_id_to_old_id_map[new_id];
                                old_id_to_new_id_map[old_id] = newnew_id;
                                newnew_id_to_old_id_map[newnew_id] = old_id;
                            }
                            new_id_to_old_id_map = newnew_id_to_old_id_map;
                            igraph_vector_int_destroy(&newnew_id_to_new_id_map);
                            num_nodes_removed ++;
                        }
                        if(igraph_vector_int_empty(&nodes_to_keep)) {
                            // this cluster completely disintegrated somehow
                            // i don't think it'll ever reach 1 before it reaches 2 and does 1 node in both partitions
                            break;
                        }
                    }
                }
                if(is_well_connected) {
                    /* std::cerr << "cluster size " << std::to_string(current_cluster.size()) <<  " well connected after performing " << std::to_string(num_nodes_removed) << " trivial mincuts " << std::endl; */
                    current_cluster = std::vector(current_cluster_set.begin(), current_cluster_set.end());
                    {
                        std::lock_guard<std::mutex> done_being_clustered_guard(CM::done_being_clustered_mutex);
                        CM::done_being_clustered_clusters.push(current_cluster);
                    }
                } else if(is_non_trivial_cut) {
                    /* std::cerr << "cluster mincut into " << std::to_string(in_partition.size()) << ":" << std::to_string(out_partition.size()) << " after performing " << std::to_string(num_nodes_removed) << " trivial mincuts " << std::endl; */
                    if(in_partition.size() > 1) {
                        std::vector<std::vector<int>> in_clusters = CM::RunClusterOnPartition(&induced_subgraph, algorithm, seed, clustering_parameter, in_partition);
                        for(size_t i = 0; i < in_clusters.size(); i ++) {
                            std::vector<int> translated_in_clusters;
                            for(size_t j = 0; j < in_clusters[i].size(); j ++) {
                                translated_in_clusters.push_back(new_id_to_old_id_map[in_clusters[i][j]]);
                            }
                            {
                                std::lock_guard<std::mutex> to_be_clustered_guard(CM::to_be_clustered_mutex);
                                CM::to_be_clustered_clusters.push(translated_in_clusters);
                            }
                        }
                    }
                    if(out_partition.size() > 1) {
                        std::vector<std::vector<int>> out_clusters = CM::RunClusterOnPartition(&induced_subgraph, algorithm, seed, clustering_parameter, out_partition);
                        for(size_t i = 0; i < out_clusters.size(); i ++) {
                            std::vector<int> translated_out_clusters;
                            for(size_t j = 0; j < out_clusters[i].size(); j ++) {
                                translated_out_clusters.push_back(new_id_to_old_id_map[out_clusters[i][j]]);
                            }
                            {
                                std::lock_guard<std::mutex> to_be_clustered_guard(CM::to_be_clustered_mutex);
                                CM::to_be_clustered_clusters.push(translated_out_clusters);
                            }
                        }
                    }
                }
                igraph_vector_int_destroy(&nodes_to_keep);
                igraph_vector_int_destroy(&new_id_to_old_id_vector_map);
                igraph_destroy(&induced_subgraph);
            }
        }
    private:
        static inline std::mutex to_be_mincut_mutex;
        static inline std::condition_variable to_be_mincut_condition_variable;
        static inline std::queue<std::vector<int>> to_be_mincut_clusters;
        static inline std::mutex to_be_clustered_mutex;
        static inline std::queue<std::vector<int>> to_be_clustered_clusters;
        static inline std::mutex done_being_clustered_mutex;
        static inline std::queue<std::vector<int>> done_being_clustered_clusters;
};

#endif
