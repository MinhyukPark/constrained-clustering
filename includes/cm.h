#ifndef CM_H
#define CM_H
#include "constrained.h"


class CM : public ConstrainedClustering {
    public:
        CM(std::string edgelist, std::string algorithm, double clustering_parameter, bool start_with_clustering, int num_processors, std::string output_file, std::string log_file, int log_level) : ConstrainedClustering(edgelist, algorithm, clustering_parameter, start_with_clustering, num_processors, output_file, log_file, log_level) {
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
                /* to_be_mincut_condition_variable.wait(to_be_mincut_lock, []() { */
                /*     return !CM::to_be_mincut_clusters.empty(); */
                /* }); */
                std::vector<int> current_cluster = CM::to_be_mincut_clusters.front();
                CM::to_be_mincut_clusters.pop();
                to_be_mincut_lock.unlock();
                if(current_cluster.size() == 1 && current_cluster[0] == -1) {
                    // done with work!
                    return;
                }
                std::cerr << "Checking mincut on a cluster" << std::endl;
                igraph_vector_int_t nodes_to_keep;
                igraph_vector_int_t new_id_to_old_id_map;
                igraph_vector_int_init(&new_id_to_old_id_map, current_cluster.size());
                igraph_vector_int_init(&nodes_to_keep, current_cluster.size());
                for(size_t i = 0; i < current_cluster.size(); i ++) {
                    VECTOR(nodes_to_keep)[i] = current_cluster[i];
                }
                igraph_t induced_subgraph;
                igraph_induced_subgraph_map(graph, &induced_subgraph, igraph_vss_vector(&nodes_to_keep), IGRAPH_SUBGRAPH_CREATE_FROM_SCRATCH, NULL, &new_id_to_old_id_map);
                MinCutCustom mcc(&induced_subgraph);
                int edge_cut_size = mcc.ComputeMinCut();
                std::vector<int> in_partition = mcc.GetInPartition();
                std::vector<int> out_partition = mcc.GetOutPartition();
                bool is_well_connected = ConstrainedClustering::IsWellConnected(in_partition, out_partition, edge_cut_size, &induced_subgraph);
                std::cerr << "mincut size was " << std::to_string(edge_cut_size) << std::endl;
                if(is_well_connected) {
                    std::cerr << "cluster is well connected" << std::endl;
                    {
                        std::lock_guard<std::mutex> done_being_clustered_guard(CM::done_being_clustered_mutex);
                        CM::done_being_clustered_clusters.push(current_cluster);
                    }
                } else {
                    std::cerr << "cluster is not well connected" << std::endl;
                    std::cerr << "split into " << std::to_string(in_partition.size()) << " and " << std::to_string(out_partition.size()) << std::endl;
                    /* for(size_t i = 0; i < in_partition.size(); i ++) { */
                    /*     in_partition[i] = VECTOR(new_id_to_old_id_map)[in_partition[i]]; */
                    /* } */
                    /* for(size_t i = 0; i < out_partition.size(); i ++) { */
                    /*     out_partition[i] = VECTOR(new_id_to_old_id_map)[out_partition[i]]; */
                    /* } */
                    if(in_partition.size() > 1) {
                        std::vector<std::vector<int>> in_clusters = CM::RunClusterOnPartition(&induced_subgraph, algorithm, seed, clustering_parameter, in_partition);
                        {
                            std::lock_guard<std::mutex> to_be_clustered_guard(CM::to_be_clustered_mutex);
                            for(size_t i = 0; i < in_clusters.size(); i ++) {
                                std::vector<int> translated_in_clusters;
                                for(size_t j = 0; j < in_clusters[i].size(); j ++) {
                                    translated_in_clusters.push_back(VECTOR(new_id_to_old_id_map)[in_clusters[i][j]]);
                                }
                                CM::to_be_clustered_clusters.push(translated_in_clusters);
                            }
                        }
                    }
                    if(out_partition.size() > 1) {
                        std::vector<std::vector<int>> out_clusters = CM::RunClusterOnPartition(&induced_subgraph, algorithm, seed, clustering_parameter, out_partition);
                        {
                            std::lock_guard<std::mutex> to_be_clustered_guard(CM::to_be_clustered_mutex);
                            for(size_t i = 0; i < out_clusters.size(); i ++) {
                                std::vector<int> translated_out_clusters;
                                for(size_t j = 0; j < out_clusters[i].size(); j ++) {
                                    translated_out_clusters.push_back(VECTOR(new_id_to_old_id_map)[out_clusters[i][j]]);
                                }
                                CM::to_be_clustered_clusters.push(translated_out_clusters);
                            }
                        }
                    }
                }
                igraph_vector_int_destroy(&nodes_to_keep);
                igraph_vector_int_destroy(&new_id_to_old_id_map);
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
