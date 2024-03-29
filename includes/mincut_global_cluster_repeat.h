#ifndef MINCUT_GLOBAL_CLUSTER_REPEAT_H
#define MINCUT_GLOBAL_CLUSTER_REPEAT_H
#include "constrained.h"


class MinCutGlobalClusterRepeat : public ConstrainedClustering {
    public:
        MinCutGlobalClusterRepeat(std::string edgelist, std::string algorithm, double clustering_parameter, bool start_with_clustering, std::string existing_clustering, int num_processors, std::string output_file, std::string log_file, int log_level) : ConstrainedClustering(edgelist, algorithm, clustering_parameter, start_with_clustering, existing_clustering, num_processors, output_file, log_file, log_level) {
        };
        int main() override;

        static inline void MinCutWorker(igraph_t* graph) {
            while (true) {
                std::unique_lock<std::mutex> to_be_mincut_lock{to_be_mincut_mutex};
                std::vector<int> current_cluster = MinCutGlobalClusterRepeat::to_be_mincut_clusters.front();
                MinCutGlobalClusterRepeat::to_be_mincut_clusters.pop();
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
                std::vector<int> in_partition = mcc.GetInPartition();
                std::vector<int> out_partition = mcc.GetOutPartition();
                bool is_well_connected = ConstrainedClustering::IsWellConnected(in_partition, out_partition, edge_cut_size, &induced_subgraph);
                if(is_well_connected) {
                    {
                        std::lock_guard<std::mutex> done_being_clustered_guard(MinCutGlobalClusterRepeat::done_being_clustered_mutex);
                        MinCutGlobalClusterRepeat::done_being_clustered_clusters.push(current_cluster);
                    }
                } else {
                    if(in_partition.size() > 1) {
                        for(size_t i = 0; i < in_partition.size(); i ++) {
                            in_partition[i] = VECTOR(new_id_to_old_id_map)[in_partition[i]];
                        }
                        {
                            std::lock_guard<std::mutex> to_be_clustered_guard(MinCutGlobalClusterRepeat::to_be_clustered_mutex);
                            MinCutGlobalClusterRepeat::to_be_clustered_clusters.push(in_partition);
                        }
                    }
                    if(out_partition.size() > 1) {
                        for(size_t i = 0; i < out_partition.size(); i ++) {
                            out_partition[i] = VECTOR(new_id_to_old_id_map)[out_partition[i]];
                        }
                        {
                            std::lock_guard<std::mutex> to_be_clustered_guard(MinCutGlobalClusterRepeat::to_be_clustered_mutex);
                            MinCutGlobalClusterRepeat::to_be_clustered_clusters.push(out_partition);
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
