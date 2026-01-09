#ifndef CM_H
#define CM_H
#include "constrained.h"


class CM : public ConstrainedClustering {
    public:
        CM(std::string edgelist, std::string algorithm, double clustering_parameter, std::string existing_clustering, int num_processors, std::string output_file, std::string log_file, std::string history_file, int log_level, std::string connectedness_criterion, bool prune, std::string mincut_type) : ConstrainedClustering(edgelist, algorithm, clustering_parameter, existing_clustering, num_processors, output_file, log_file, history_file, log_level, connectedness_criterion, mincut_type) {
        };
        int main() override;

        static inline std::vector<std::vector<int>> RunClusterOnPartition(const igraph_t* graph, std::string algorithm, int seed, double clustering_parameter, std::vector<int>& partition) {
            std::vector<std::vector<int>> cluster_vectors;
            std::map<int, std::vector<int>> cluster_map;
            igraph_vector_int_t nodes_to_keep;
            igraph_vector_int_t new_id_to_old_id_map; // here, new_id is the id of the current half, old_id is the id of the induced_subgraph of the cluster (both halves)
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

        static inline void MinCutOrClusterWorker(const igraph_t* graph, std::string algorithm, int seed, double clustering_parameter, ConnectednessCriterion current_connectedness_criterion, double connectedness_criterion_c, double connectedness_criterion_x, double pre_computed_log, bool prune, std::string mincut_type = "cactus") {
            while (true) {
                std::unique_lock<std::mutex> to_be_mincut_lock{to_be_mincut_mutex};
                std::pair<std::vector<int>, int> current_front = CM::to_be_mincut_clusters.front();
                std::vector<int> current_cluster = current_front.first;
                int current_cluster_id = current_front.second;
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
                igraph_vector_int_init(&new_id_to_old_id_vector_map, igraph_vector_int_size(&nodes_to_keep));
                igraph_induced_subgraph_map(graph, &induced_subgraph, igraph_vss_vector(&nodes_to_keep), IGRAPH_SUBGRAPH_CREATE_FROM_SCRATCH, NULL, &new_id_to_old_id_vector_map);
                std::map<int, int> new_id_to_old_id_map; // new_id = induced subgraph id, old_id = global graph id
                std::map<int, int> old_id_to_new_id_map;
                for(int i = 0; i < igraph_vector_int_size(&new_id_to_old_id_vector_map); i ++) {
                    new_id_to_old_id_map[i] = VECTOR(new_id_to_old_id_vector_map)[i];
                    old_id_to_new_id_map[VECTOR(new_id_to_old_id_vector_map)[i]] = i;
                }
                /* while(!(is_non_trivial_cut || is_well_connected)) { */
                // we do mincuts until it's a non trivial mincut or if the current cluster is already well connected (perhaps after some trivial mincuts)
                while(true) {
                    MinCutCustom mcc(&induced_subgraph, mincut_type);
                    edge_cut_size = mcc.ComputeMinCut();
                    std::vector<int> in_partition_candidate = mcc.GetInPartition();
                    std::vector<int> out_partition_candidate = mcc.GetOutPartition();
                    is_well_connected = ConstrainedClustering::IsWellConnected(current_connectedness_criterion, connectedness_criterion_c, connectedness_criterion_x, pre_computed_log, in_partition_candidate.size(), out_partition_candidate.size(), edge_cut_size);
                    is_non_trivial_cut = in_partition_candidate.size() > 1 && out_partition_candidate.size() > 1;
                    if(prune || is_well_connected || is_non_trivial_cut) {
                        in_partition = in_partition_candidate;
                        out_partition = out_partition_candidate;
                        break;
                    } else {
                        // since we're removing a node, the new_id (induced subgraph id) to old_id mapping needs to change
                        // this is a temp map that will replace the newnew_id_to_old_id_map everytime the induced subgraph is changed
                        std::map<int, int> newnew_id_to_old_id_map;
                        std::vector<int> partitions[] = {in_partition_candidate, out_partition_candidate};
                        int current_cluster_size = in_partition_candidate.size() + out_partition_candidate.size();
                        for(int partition_id = 0; partition_id < 2; partition_id ++) {
                            std::vector<int> current_partition = partitions[partition_id];
                            if(current_partition.size() == 1) {
                                int node_to_remove = -1;
                                igraph_vector_int_t newnew_id_to_new_id_map;
                                igraph_vector_int_init(&newnew_id_to_new_id_map, current_cluster_size - 1);
                                node_to_remove = current_partition.at(0);
                                current_cluster_set.erase(new_id_to_old_id_map[node_to_remove]);
                                // MARK: possibly changes behavior?
                                // igraph_delete_vertices_idx(&induced_subgraph, igraph_vss_1(node_to_remove), NULL, &newnew_id_to_new_id_map);
                                igraph_delete_vertices_map(&induced_subgraph, igraph_vss_1(node_to_remove), NULL, &newnew_id_to_new_id_map);
                                for(int i = 0; i < igraph_vector_int_size(&newnew_id_to_new_id_map); i ++) {
                                    int newnew_id = i;
                                    int new_id = VECTOR(newnew_id_to_new_id_map)[newnew_id];
                                    int old_id = new_id_to_old_id_map[new_id];
                                    old_id_to_new_id_map[old_id] = newnew_id;
                                    newnew_id_to_old_id_map[newnew_id] = old_id;
                                }
                                new_id_to_old_id_map = newnew_id_to_old_id_map;
                                igraph_vector_int_destroy(&newnew_id_to_new_id_map);
                                current_cluster_size --;
                            }
                        }
                        if(current_cluster_size == 0) {
                            // this cluster completely disintegrated somehow?
                            break;
                        }
                    }
                }
                // by this point, the cluster is either well-connected or we actually have a mincut we need to do
                if(is_well_connected) {
                    // just return the cluster without using the mincut
                    current_cluster = std::vector(current_cluster_set.begin(), current_cluster_set.end());
                    {
                        std::lock_guard<std::mutex> done_being_clustered_guard(CM::done_being_clustered_mutex);
                        CM::done_being_clustered_clusters.push({current_cluster, current_cluster_id});
                    }
                } else {
                    // do the mincut and actually run a clustering algorithm on both sides
                    std::vector<int> partitions[] = {in_partition, out_partition};
                    for(int partition_id = 0; partition_id < 2; partition_id ++) {
                        std::vector<int> current_partition = partitions[partition_id];
                        // assert(current_partition.size() > 1);
                        if (current_partition.size() > 1) {
                            std::vector<std::vector<int>> current_clusters = CM::RunClusterOnPartition(&induced_subgraph, algorithm, seed, clustering_parameter, current_partition);
                            for(size_t i = 0; i < current_clusters.size(); i ++) {
                                std::vector<int> translated_current_clusters;
                                for(size_t j = 0; j < current_clusters[i].size(); j ++) {
                                    translated_current_clusters.push_back(new_id_to_old_id_map[current_clusters[i][j]]);
                                }
                                {
                                    std::lock_guard<std::mutex> to_be_clustered_guard(CM::to_be_clustered_mutex);
                                    CM::to_be_clustered_clusters.push({translated_current_clusters, current_cluster_id});
                                }
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
        bool prune;
        static inline std::mutex to_be_mincut_mutex;
        static inline std::condition_variable to_be_mincut_condition_variable;
        static inline std::queue<std::pair<std::vector<int>, int>> to_be_mincut_clusters;
        static inline std::mutex to_be_clustered_mutex;
        static inline std::queue<std::pair<std::vector<int>, int>> to_be_clustered_clusters;
        static inline std::mutex done_being_clustered_mutex;
        static inline std::queue<std::pair<std::vector<int>, int>> done_being_clustered_clusters;
};

#endif
