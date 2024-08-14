#ifndef CONSTRAINED_H
#define CONSTRAINED_H
// mincut_custom.h needs to be included first because of string override
#include "mincut_custom.h"

#include <cmath>
#include <chrono>
#include <condition_variable>
#include <random>
#include <thread>
#include <map>
#include <fstream>

#include <libleidenalg/GraphHelper.h>
#include <libleidenalg/Optimiser.h>
#include <libleidenalg/CPMVertexPartition.h>
#include <libleidenalg/ModularityVertexPartition.h>

enum Log {info, debug, error = -1};

class ConstrainedClustering {
    public:
        ConstrainedClustering(std::string edgelist, std::string algorithm, double clustering_parameter, std::string existing_clustering, int num_processors, std::string output_file, std::string log_file, int log_level) : edgelist(edgelist), algorithm(algorithm), clustering_parameter(clustering_parameter), existing_clustering(existing_clustering), num_processors(num_processors), output_file(output_file), log_file(log_file), log_level(log_level) {
            if(this->log_level > -1) {
                this->start_time = std::chrono::steady_clock::now();
                this->log_file_handle.open(this->log_file);
            }
            this->num_calls_to_log_write = 0;
        };

        virtual ~ConstrainedClustering() {
            if(this->log_level > -1) {
                this->log_file_handle.close();
            }
        }

        virtual int main() = 0;
        int WriteToLogFile(std::string message, Log message_type);
        void WritePartitionMap(std::map<int,int>& final_partition);
        void WriteClusterQueue(std::queue<std::vector<int>>& to_be_clustered_clusters, igraph_t* graph);

        static inline std::map<std::string, int> GetOriginalToNewIdMap(igraph_t* graph) {
            std::map<std::string, int> original_to_new_id_map;
            /* igraph_vit_t vit; */
            /* igraph_vit_create(graph, igraph_vss_all(), &vit); */
            /* for(; !IGRAPH_VIT_END(vit); IGRAPH_VIT_NEXT(vit)) { */
            /*     igraph_integer_t current_node = IGRAPH_VIT_GET(vit); */
            /*     original_to_new_id_map[VAS(graph, "name", current_node)] = current_node; */
            /* } */
            /* igraph_vit_destroy(&vit); */
            for(int node_id = 0; node_id < igraph_vcount(graph); node_id ++) {
                original_to_new_id_map[VAS(graph, "name", node_id)] = node_id;
            }
            return original_to_new_id_map;
        }

        static inline std::map<int, int> ReadCommunities(const std::map<std::string, int>& original_to_new_id_map, std::string existing_clustering) {
            std::map<int, int> partition_map;
            std::ifstream existing_clustering_file(existing_clustering);
            std::string node_id;
            int cluster_id = -1;
            while (existing_clustering_file >> node_id >> cluster_id) {
                int new_node_id = original_to_new_id_map.at(node_id);
                partition_map[new_node_id] = cluster_id;
            }
            return partition_map;
        }

        static inline std::map<int, int> ReadCommunities(std::string existing_clustering) {
            std::map<int, int> partition_map;
            std::ifstream existing_clustering_file(existing_clustering);
            int node_id = -1;
            int cluster_id = -1;
            while (existing_clustering_file >> node_id >> cluster_id) {
                partition_map[node_id] = cluster_id;
            }
            return partition_map;
        }

        static inline void ClusterQueueToMap(std::queue<std::vector<int>>& cluster_queue, std::map<int, int>& node_id_to_cluster_id_map) {
            int cluster_id = 0;
            while(!cluster_queue.empty()) {
                std::vector<int> current_cluster = cluster_queue.front();
                cluster_queue.pop();
                for(size_t i = 0; i < current_cluster.size(); i ++) {
                    node_id_to_cluster_id_map[current_cluster[i]] = cluster_id;
                }
                cluster_id ++;
            }
        }

        // currently keeps only those edges that go from within these clusters defined in the map
        static inline void RemoveInterClusterEdges(igraph_t* graph, const std::map<int,int>& node_id_to_cluster_id_map) {
            igraph_vector_int_t edges_to_remove;
            igraph_vector_int_init(&edges_to_remove, 0);
            igraph_eit_t eit;
            igraph_eit_create(graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
            for(; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
                igraph_integer_t current_edge = IGRAPH_EIT_GET(eit);
                int from_node = IGRAPH_FROM(graph, current_edge);
                int to_node = IGRAPH_TO(graph, current_edge);
                if(node_id_to_cluster_id_map.contains(from_node) && node_id_to_cluster_id_map.contains(to_node)
                    && (node_id_to_cluster_id_map.at(from_node) == node_id_to_cluster_id_map.at(to_node))) {
                    // keep the edge
                } else {
                    igraph_vector_int_push_back(&edges_to_remove, IGRAPH_EIT_GET(eit));
                    /* std::cerr << "removing edge " << from_node << "-" << to_node << std::endl; */
                    /* if(node_id_to_cluster_id_map.contains(from_node) && node_id_to_cluster_id_map.contains(to_node)) { */
                        /* std::cerr << VAS(graph, "name", from_node) << " in cluster " << node_id_to_cluster_id_map.at(from_node) << " and " << VAS(graph, "name", to_node) << " in cluster " << node_id_to_cluster_id_map.at(to_node) << std::endl; */
                    /* } else { */
                    /*     std::cerr << "one of the end points not in any clusters" << std::endl; */
                    /* } */
                }
            }
            igraph_es_t es;
            igraph_es_vector_copy(&es, &edges_to_remove);
            igraph_delete_edges(graph, es);
            igraph_eit_destroy(&eit);
            igraph_es_destroy(&es);
            igraph_vector_int_destroy(&edges_to_remove);
        }

        static inline void RestoreInterClusterEdges(const igraph_t* original_graph, igraph_t* graph, const std::map<int,int>& node_id_to_cluster_id_map) {
            struct ClusterEdgeStruct {
                int cluster_id;
                int other_cluster_id;
                double intercluster_num_edges;
                double cluster_sum_edges;
                bool operator<(const struct ClusterEdgeStruct& other) const {
                    return (this->intercluster_num_edges / this->cluster_sum_edges) < (other.intercluster_num_edges / other.cluster_sum_edges);
                }
            };

            std::map<int, std::vector<int>> cluster_id_to_node_vec_map;
            std::map<int, std::set<int>> cluster_id_to_node_set_map;
            std::map<int, int> cluster_id_to_num_edges_map;
            std::map<int, std::map<int, int>> cluster_id_to_cluster_id_to_num_edges_map; // number of edges that go from key cluster id to val-key cluster id
            std::priority_queue<ClusterEdgeStruct> pq;
            std::map<int, std::vector<int>> clusters_to_be_merged;
            std::set<int> clusters_already_chosen_to_be_merged;
            for(auto const& [node_id, cluster_id]: node_id_to_cluster_id_map) {
                cluster_id_to_node_vec_map[cluster_id].push_back(node_id);
                cluster_id_to_node_set_map[cluster_id].insert(node_id);
            }

            igraph_eit_t eit;
            igraph_eit_create(original_graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
            for(; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
                igraph_integer_t current_edge = IGRAPH_EIT_GET(eit);
                int from_node = IGRAPH_FROM(original_graph, current_edge);
                int to_node = IGRAPH_TO(original_graph, current_edge);
                for(auto const& [cluster_id, node_id_set]: cluster_id_to_node_set_map) {
                    if(node_id_set.contains(from_node) && node_id_set.contains(to_node)) {
                        cluster_id_to_num_edges_map[cluster_id] += 1;
                    }
                    for(auto const& [other_cluster_id, other_node_id_set]: cluster_id_to_node_set_map) {
                        if((node_id_set.contains(from_node) && other_node_id_set.contains(to_node)) || (node_id_set.contains(to_node) && other_node_id_set.contains(from_node))) {
                            cluster_id_to_cluster_id_to_num_edges_map[cluster_id][other_cluster_id] += 1;
                        }
                    }
                }
            }
            igraph_eit_destroy(&eit);

            for(auto const& [cluster_id, other_cluster_id_to_num_edges_map]: cluster_id_to_cluster_id_to_num_edges_map) {
                // add the matrix thing
                for(auto const& [other_cluster_id, num_edges_from_cluster_id_to_other_cluster_id]: other_cluster_id_to_num_edges_map) {
                    int current_cluster_id_num_edges = cluster_id_to_num_edges_map[cluster_id];
                    int other_cluster_id_num_edges = cluster_id_to_num_edges_map[other_cluster_id];
                    double intercluster_num_edges = num_edges_from_cluster_id_to_other_cluster_id;
                    double cluster_sum_edges = current_cluster_id_num_edges + other_cluster_id_num_edges;
                    double threshold_value = 4;
                    if((current_cluster_id_num_edges / threshold_value > intercluster_num_edges) && (other_cluster_id_num_edges / threshold_value > intercluster_num_edges)) {
                        pq.push(ClusterEdgeStruct{.cluster_id=cluster_id, .other_cluster_id=other_cluster_id, .intercluster_num_edges=intercluster_num_edges, .cluster_sum_edges=cluster_sum_edges});
                    }
                }
            }
            // algorithm for not greedy approach
            while(!pq.empty()) {
                ClusterEdgeStruct current_cluster_edge_struct = pq.top();
                pq.pop();
                int cluster_id = current_cluster_edge_struct.cluster_id;
                int other_cluster_id = current_cluster_edge_struct.other_cluster_id;
                if(clusters_already_chosen_to_be_merged.contains(cluster_id) || clusters_already_chosen_to_be_merged.contains(other_cluster_id)) {
                } else {
                    clusters_already_chosen_to_be_merged.insert(cluster_id);
                    clusters_already_chosen_to_be_merged.insert(other_cluster_id);
                }
                ConstrainedClustering::RestoreInterClusterEdges(original_graph, graph, node_id_to_cluster_id_map, cluster_id, other_cluster_id);
            }
        }
        static inline void RestoreInterClusterEdges(const igraph_t* original_graph, igraph_t* graph, const std::map<int,int>& node_id_to_cluster_id_map, int cluster_id, int other_cluster_id) {
            igraph_eit_t eit;
            igraph_eit_create(original_graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
            for(; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
                igraph_integer_t current_edge = IGRAPH_EIT_GET(eit);
                int from_node = IGRAPH_FROM(original_graph, current_edge);
                int to_node = IGRAPH_TO(original_graph, current_edge);
                if(node_id_to_cluster_id_map.at(from_node) == cluster_id &&  node_id_to_cluster_id_map.at(to_node) == other_cluster_id) {
                    igraph_add_edge(graph, from_node, to_node);
                }
                if(node_id_to_cluster_id_map.at(to_node) == cluster_id &&  node_id_to_cluster_id_map.at(from_node) == other_cluster_id) {
                    igraph_add_edge(graph, to_node, from_node);
                }
            }
            igraph_eit_destroy(&eit);
        }


        static inline void RunLouvainAndUpdatePartition(std::map<int, int>& partition_map, int seed, igraph_t* graph) {
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

        static inline void RunLeidenAndUpdatePartition(std::map<int, int>& partition_map, MutableVertexPartition* partition, int seed, igraph_t* graph, int num_iter= 2) {
            Optimiser o;
            o.set_rng_seed(seed);
            for(int i = 0; i < num_iter; i ++) {
                o.optimise_partition(partition);
            }
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

        static inline void SetIgraphAllEdgesWeight(igraph_t* graph, double weight) {
            igraph_eit_t eit;
            igraph_eit_create(graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
            for(; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
                SETEAN(graph, "weight", IGRAPH_EIT_GET(eit), 1);
            }
            igraph_eit_destroy(&eit);
        }

        static inline std::map<int, int> GetCommunities(std::string edgelist, std::string algorithm, int seed, double clustering_parameter, igraph_t* graph_ptr) {
            std::map<int, int> partition_map;
            igraph_t graph;
            if(graph_ptr == NULL) {
                FILE* edgelist_file = fopen(edgelist.c_str(), "r");
                igraph_set_attribute_table(&igraph_cattribute_table);
                igraph_read_graph_ncol(&graph, edgelist_file, NULL, 1, IGRAPH_ADD_WEIGHTS_IF_PRESENT, IGRAPH_UNDIRECTED);
                /* igraph_read_graph_edgelist(&graph, edgelist_file, 0, false); */
                fclose(edgelist_file);
                /* ConstrainedClustering::SetIgraphAllEdgesWeight(&graph, 1); */

            } else {
                graph = *graph_ptr;
            }

            if(algorithm == "louvain") {
                ConstrainedClustering::RunLouvainAndUpdatePartition(partition_map, seed, &graph);
            } else if(algorithm == "leiden-cpm") {
                std::vector<double> edge_weights;
                igraph_eit_t eit;
                igraph_eit_create(&graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
                for(; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
                    double current_edge_weight = EAN(&graph, "weight", IGRAPH_EIT_GET(eit));
                    /* std::cerr << "current edge weight: " << current_edge_weight << std::endl; */
                    edge_weights.push_back(current_edge_weight);
                }
                igraph_eit_destroy(&eit);
                Graph* leiden_graph = Graph::GraphFromEdgeWeights(&graph, edge_weights);
                CPMVertexPartition partition(leiden_graph, clustering_parameter);
                ConstrainedClustering::RunLeidenAndUpdatePartition(partition_map, &partition, seed, &graph);
            } else if(algorithm == "leiden-mod") {
                std::vector<double> edge_weights;
                igraph_eit_t eit;
                igraph_eit_create(&graph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
                for(; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
                    double current_edge_weight = EAN(&graph, "weight", IGRAPH_EIT_GET(eit));
                    edge_weights.push_back(current_edge_weight);
                }
                igraph_eit_destroy(&eit);
                Graph* leiden_graph = Graph::GraphFromEdgeWeights(&graph, edge_weights);
                ModularityVertexPartition partition(leiden_graph);
                ConstrainedClustering::RunLeidenAndUpdatePartition(partition_map, &partition, seed, &graph);
            } else {
                throw std::invalid_argument("GetCommunities(): Unsupported algorithm");
            }

            if(graph_ptr == NULL) {
                igraph_destroy(&graph);
            }
            return partition_map;
        }

        static inline std::vector<std::vector<int>> GetConnectedComponents(igraph_t* graph_ptr) {
            std::vector<std::vector<int>> connected_components_vector;
            std::map<int, std::vector<int>> component_id_to_member_vector_map;
            igraph_vector_int_t component_id_vector;
            igraph_vector_int_init(&component_id_vector, 0);
            igraph_vector_int_t membership_size_vector;
            igraph_vector_int_init(&membership_size_vector, 0);
            igraph_integer_t number_of_components;
            igraph_connected_components(graph_ptr, &component_id_vector, &membership_size_vector, &number_of_components, IGRAPH_WEAK);
            /* std::cerr << "num con comp: " << number_of_components << std::endl; */
            for(int node_id = 0; node_id < igraph_vcount(graph_ptr); node_id ++) {
                int current_component_id = VECTOR(component_id_vector)[node_id];
                /* std::cerr << "component id: " << current_component_id << std::endl; */
                /* std::cerr << "component size: " << VECTOR(membership_size_vector)[current_component_id] << std::endl; */
                /* std::cerr << "graph node id: " << node_id << std::endl; */
                /* std::cerr << "original node id: " << VAS(graph_ptr, "name", node_id) << std::endl; */
                if(VECTOR(membership_size_vector)[current_component_id] > 1) {
                    component_id_to_member_vector_map[current_component_id].push_back(node_id);
                }
            }
            igraph_vector_int_destroy(&component_id_vector);
            igraph_vector_int_destroy(&membership_size_vector);
            for(auto const& [component_id, member_vector] : component_id_to_member_vector_map) {
                connected_components_vector.push_back(member_vector);
            }
            return connected_components_vector;
        }

        static inline bool IsConnected(int edge_cut_size) {
            return edge_cut_size >= 1;
        }

        static inline bool IsWellConnected(int in_partition_size, int out_partition_size, int edge_cut_size) {
            bool node_connectivity = log10(in_partition_size + out_partition_size) < edge_cut_size;
            /* return edge_connectivity && node_connectivity; */
            return node_connectivity;
        }

        static inline bool IsWellConnected(const std::vector<int>& in_partition, const std::vector<int>& out_partition, int edge_cut_size, const igraph_t* induced_subgraph) {
            if(edge_cut_size == 0) {
                return false;
            }
            /* double threshold_value = 4; */
            /* double num_edge_in_side = 0; */
            /* double num_edge_out_side = 0; */
            /* if(edge_cut_size != 0) { */
            /*     std::set<int> in_partition_set(in_partition.begin(), in_partition.end()); */
            /*     std::set<int> out_partition_set(out_partition.begin(), out_partition.end()); */
            /*     igraph_eit_t eit; */
            /*     igraph_eit_create(induced_subgraph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit); */
            /*     for(; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) { */
            /*         igraph_integer_t current_edge = IGRAPH_EIT_GET(eit); */
            /*         int from_node = IGRAPH_FROM(induced_subgraph, current_edge); */
            /*         int to_node = IGRAPH_TO(induced_subgraph, current_edge); */
            /*         if(in_partition_set.contains(from_node) && in_partition_set.contains(to_node)) { */
            /*             num_edge_in_side ++; */
            /*         } */
            /*         if(out_partition_set.contains(from_node) && out_partition_set.contains(to_node)) { */
            /*             num_edge_out_side ++; */
            /*         } */
            /*     } */
            /*     igraph_eit_destroy(&eit); */
            /* } */

            /* bool edge_connectivity = (num_edge_in_side / threshold_value > edge_cut_size) && (num_edge_out_side / threshold_value > edge_cut_size); */
            /* std::cerr << edge_cut_size << std::endl; */
            bool node_connectivity = log10(in_partition.size() + out_partition.size()) < edge_cut_size;
            /* return edge_connectivity && node_connectivity; */
            return node_connectivity;
        }

    protected:
        std::string edgelist;
        std::string algorithm;
        double clustering_parameter;
        std::string existing_clustering;
        int num_processors;
        std::string output_file;
        std::string log_file;
        std::chrono::steady_clock::time_point start_time;
        std::ofstream log_file_handle;
        int log_level;
        int num_calls_to_log_write;
};

#endif
