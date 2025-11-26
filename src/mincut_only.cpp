#include "mincut_only.h"


int MincutOnly::main() {

    this->WriteToLogFile("Parsing connectedness criterion" , Log::info);
/* F(n) = C log_x(n), where C and x are parameters specified by the user (our default is C=1 and x=10) */
/* G(n) = C n^x, where C and x are parameters specified by the user (here, presumably 0<x<2). Note that x=1 makes it linear. */
        /* static inline bool IsWellConnected(std::string connectedness_criterion, int in_partition_size, int out_partition_size, int edge_cut_size) { */
    double connectedness_criterion_c = this->connectedness_criterion_c;
    double connectedness_criterion_x = this->connectedness_criterion_x;
    double pre_computed_log = this->pre_computed_log;
    ConnectednessCriterion current_connectedness_criterion = this->current_connectedness_criterion;
    this->WriteToLogFile("Loading the initial graph" , Log::info);


    std::map<std::string, int> original_to_new_id_map = this->GetOriginalToNewIdMap(this->edgelist);
    std::map<int, std::string> new_to_originial_id_map = this->InvertMap(original_to_new_id_map);
    igraph_t graph;
    igraph_empty(&graph, 0, IGRAPH_UNDIRECTED);
    this->LoadEdgesFromFile(&graph, this->edgelist, original_to_new_id_map);
    this->WriteToLogFile("Finished loading the initial graph" , Log::info);

    int before_mincut_number_of_clusters = -1;
    int after_mincut_number_of_clusters = -2;
    int iter_count = 0;

    std::map<int, int> new_node_id_to_cluster_id_map = ConstrainedClustering::ReadCommunities(original_to_new_id_map, this->existing_clustering);
    /* std::cerr << "num nodes in mapping: " << original_to_new_id_map.size()  << std::endl; */
    /* std::cerr << "num nodes read from clustering: " << new_node_id_to_cluster_id_map.size()  << std::endl; */
    /* for (auto const& [node_id, cluster_id] : new_node_id_to_cluster_id_map) { */
    /*     std::cerr << std::to_string(node_id) << "," << std::to_string(cluster_id)  << std::endl; */
    /* } */
    ConstrainedClustering::RemoveInterClusterEdges(&graph, new_node_id_to_cluster_id_map);
    /* std::cerr << "num nodes after intercluster removal: " << igraph_vcount(&graph)  << std::endl; */
    /* std::cerr << "num edges after intercluster removal: " << igraph_ecount(&graph)  << std::endl; */

    /** SECTION Get Connected Components START **/
    std::vector<std::vector<int>> connected_components_vector = ConstrainedClustering::GetConnectedComponents(&graph);
    /** SECTION Get Connected Components END **/
    if(current_connectedness_criterion == ConnectednessCriterion::Simple) {
        for(size_t i = 0; i < connected_components_vector.size(); i ++) {
            MincutOnly::done_being_mincut_clusters.push(connected_components_vector[i]);
        }
    } else {
        // store the results into the queue that each thread pulls from
        for(size_t i = 0; i < connected_components_vector.size(); i ++) {
            MincutOnly::to_be_mincut_clusters.push(connected_components_vector[i]);
        }
        while (true) {
            /* std::cerr << "iter num: " << std::to_string(iter_count) << std::endl; */
            this->WriteToLogFile("Iteration number: " + std::to_string(iter_count), Log::debug);
            if(iter_count % 10000 == 0) {
                this->WriteToLogFile("Iteration number: " + std::to_string(iter_count), Log::info);
                this->WriteToLogFile(std::to_string(MincutOnly::to_be_mincut_clusters.size()) + " [connected components / clusters] to be mincut", Log::info);
            }

            /** SECTION MinCut Each Connected Component START **/
            this->WriteToLogFile(std::to_string(MincutOnly::to_be_mincut_clusters.size()) + " [connected components / clusters] to be mincut", Log::debug);
            before_mincut_number_of_clusters = MincutOnly::to_be_mincut_clusters.size();
            /* if a thread gets a cluster {-1}, then they know processing is done and they can stop working */
            /* std::cerr << "num clusters to be processed: " << std::to_string(before_mincut_number_of_clusters) << std::endl; */
            if(before_mincut_number_of_clusters > 1) {
                /* start the threads */
                for(int i = 0; i < this->num_processors; i ++) {
                    MincutOnly::to_be_mincut_clusters.push({-1});
                }
                std::vector<std::thread> thread_vector;
                for(int i = 0; i < this->num_processors; i ++) {
                    thread_vector.push_back(std::thread(MincutOnly::MinCutWorker, &graph, current_connectedness_criterion, connectedness_criterion_c, connectedness_criterion_x, pre_computed_log));
                }
                /* get the result back from threads */
                /* the results from each thread gets stored in to_be_clustered_clusters */
                for(size_t thread_index = 0; thread_index < thread_vector.size(); thread_index ++) {
                    thread_vector[thread_index].join();
                }
            } else {
                MincutOnly::to_be_mincut_clusters.push({-1});
                MincutOnly::MinCutWorker(&graph, current_connectedness_criterion, connectedness_criterion_c, connectedness_criterion_x, pre_computed_log);
            }
            this->WriteToLogFile(std::to_string(MincutOnly::to_be_mincut_clusters.size()) + " [connected components / clusters] to be mincut after a round of mincuts", Log::debug);
            /** SECTION MinCut Each Connected Component END **/

            /** SECTION Check If All Clusters Are Well-Connected START **/
            after_mincut_number_of_clusters = MincutOnly::to_be_mincut_clusters.size();
            if(after_mincut_number_of_clusters == 0) {
                this->WriteToLogFile("all clusters are (well) connected", Log::info);
                this->WriteToLogFile("Total number of iterations: " + std::to_string(iter_count + 1), Log::info);
                break;
            }
            /** SECTION Check If All Clusters Are Well-Connected END **/

            iter_count ++;
        }
    }


    this->WriteToLogFile("Writing output to: " + this->output_file, Log::info);
    this->WriteClusterQueue(MincutOnly::done_being_mincut_clusters, &graph, new_to_originial_id_map);
    igraph_destroy(&graph);
    return 0;
}
