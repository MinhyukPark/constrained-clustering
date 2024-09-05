#ifndef MINCUT_ONLY_H
#define MINCUT_ONLY_H
#include "constrained.h"


enum ConnectednessCriterion {Simple, Well};

class MincutOnly : public ConstrainedClustering {
    public:
        MincutOnly(std::string edgelist, std::string existing_clustering, int num_processors, std::string output_file, std::string log_file, ConnectednessCriterion connectedness_criterion, int log_level) : ConstrainedClustering(edgelist, "", -1, existing_clustering, num_processors, output_file, log_file, log_level), connectedness_criterion(connectedness_criterion) {
        };
        int main() override;

        static inline std::vector<std::vector<int>> GetConnectedComponentsOnPartition(const igraph_t* graph, std::vector<int>& partition) {
            std::vector<std::vector<int>> cluster_vectors;
            igraph_vector_int_t nodes_to_keep;
            igraph_vector_int_t new_id_to_old_id_map;
            igraph_vector_int_init(&new_id_to_old_id_map, partition.size());
            igraph_vector_int_init(&nodes_to_keep, partition.size());
            for(size_t i = 0; i < partition.size(); i ++) {
                VECTOR(nodes_to_keep)[i] = partition[i];
            }
            igraph_t induced_subgraph;
            igraph_induced_subgraph_map(graph, &induced_subgraph, igraph_vss_vector(&nodes_to_keep), IGRAPH_SUBGRAPH_CREATE_FROM_SCRATCH, NULL, &new_id_to_old_id_map);
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

        static inline void MinCutWorker(igraph_t* graph, ConnectednessCriterion connectedness_criterion) {
            while (true) {
                std::unique_lock<std::mutex> to_be_mincut_lock{to_be_mincut_mutex};
                to_be_mincut_condition_variable.wait(to_be_mincut_lock, []() {
                    return !MincutOnly::to_be_mincut_clusters.empty();
                });
                std::vector<int> current_cluster = MincutOnly::to_be_mincut_clusters.front();
                MincutOnly::to_be_mincut_clusters.pop();
                to_be_mincut_lock.unlock();
                if(current_cluster.size() == 1 || current_cluster[0] == -1) {
                    // done with work!
                    return;
                }
                /* std::cerr << "current cluster size: " << current_cluster.size() << std::endl; */
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

                /* igraph_vector_int_t node_degrees; */
                /* igraph_vector_int_init(&node_degrees, 0); */
                /* igraph_degree(induced_subgraph, node_degrees, igraph_vss_vector(&nodes_to_keep), IGRAPH_ALL, IGRAPH_NO_LOOPS); */
                /* for(size_t i = 0; i < ; i ++) { */
                /*     VECTOR(nodes_to_keep)[i] = current_cluster[i]; */
                /* } */
                /* while(!ConstrainedClustering::IsWellConnected */

                /* SetIgraphAllEdgesWeight(&induced_subgraph, 1.0); */
                MinCutCustom mcc(&induced_subgraph);
                int edge_cut_size = mcc.ComputeMinCut();
                std::vector<int> in_partition = mcc.GetInPartition();
                std::vector<int> out_partition = mcc.GetOutPartition();

                bool current_criterion = false;
                if(connectedness_criterion == ConnectednessCriterion::Simple) {
                    current_criterion = ConstrainedClustering::IsConnected(edge_cut_size);
                } else if(connectedness_criterion == ConnectednessCriterion::Well) {
                    current_criterion = ConstrainedClustering::IsWellConnected(in_partition, out_partition, edge_cut_size, &induced_subgraph);
                }

                if(!current_criterion) {
                    /* for(size_t i = 0; i < in_partition.size(); i ++) { */
                    /*     in_partition[i] = VECTOR(new_id_to_old_id_map)[in_partition[i]]; */
                    /* } */
                    /* for(size_t i = 0; i < out_partition.size(); i ++) { */
                    /*     out_partition[i] = VECTOR(new_id_to_old_id_map)[out_partition[i]]; */
                    /* } */
                    if(in_partition.size() > 1) {
                        std::vector<std::vector<int>> in_clusters = GetConnectedComponentsOnPartition(&induced_subgraph, in_partition);
                        for(size_t i = 0; i < in_clusters.size(); i ++) {
                            std::vector<int> translated_in_clusters;
                            for(size_t j = 0; j < in_clusters[i].size(); j ++) {
                                translated_in_clusters.push_back(VECTOR(new_id_to_old_id_map)[in_clusters[i][j]]);
                            }
                            {
                                std::lock_guard<std::mutex> to_be_mincut_guard(MincutOnly::to_be_mincut_mutex);
                                MincutOnly::to_be_mincut_clusters.push(translated_in_clusters);
                            }
                        }
                    }
                    if(out_partition.size() > 1) {
                        std::vector<std::vector<int>> out_clusters = GetConnectedComponentsOnPartition(&induced_subgraph, out_partition);
                        for(size_t i = 0; i < out_clusters.size(); i ++) {
                            std::vector<int> translated_out_clusters;
                            for(size_t j = 0; j < out_clusters[i].size(); j ++) {
                                translated_out_clusters.push_back(VECTOR(new_id_to_old_id_map)[out_clusters[i][j]]);
                            }
                            {
                                std::lock_guard<std::mutex> to_be_mincut_guard(MincutOnly::to_be_mincut_mutex);
                                MincutOnly::to_be_mincut_clusters.push(translated_out_clusters);
                            }
                        }
                    }
                } else {
                    std::unique_lock<std::mutex> done_being_mincut_lock{MincutOnly::done_being_mincut_mutex};
                    MincutOnly::done_being_mincut_clusters.push(current_cluster);
                    done_being_mincut_lock.unlock();
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
        static inline std::mutex done_being_mincut_mutex;
        static inline std::queue<std::vector<int>> done_being_mincut_clusters;
        ConnectednessCriterion connectedness_criterion;
};

#endif
