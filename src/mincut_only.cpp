#include "mincut_only.h"

int MincutOnly::main() {
    this->WriteToLogFile("Loading the initial graph" , Log::info);
    FILE* edgelist_file = fopen(this->edgelist.c_str(), "r");
    igraph_t graph;
    /* igraph_read_graph_edgelist(&graph, edgelist_file, 0, false); */
    igraph_set_attribute_table(&igraph_cattribute_table);
    igraph_read_graph_ncol(&graph, edgelist_file, NULL, 1, IGRAPH_ADD_WEIGHTS_IF_PRESENT, IGRAPH_UNDIRECTED);
    fclose(edgelist_file);
    this->WriteToLogFile("Finished loading the initial graph" , Log::info);

    int before_mincut_number_of_clusters = -1;
    int after_mincut_number_of_clusters = -2;
    int iter_count = 0;

    if(this->existing_clustering != "") {
        std::map<int, int> node_id_to_cluster_id_map = ConstrainedClustering::ReadCommunities(this->existing_clustering);
        ConstrainedClustering::RemoveInterClusterEdges(&graph, node_id_to_cluster_id_map);
    }

    /** SECTION Get Connected Components START **/
    std::vector<std::vector<int>> connected_components_vector = ConstrainedClustering::GetConnectedComponents(&graph);
    // store the results into the queue that each thread pulls from
    for(size_t i = 0; i < connected_components_vector.size(); i ++) {
        MincutOnly::to_be_mincut_clusters.push(connected_components_vector[i]);
    }
    /** SECTION Get Connected Components END **/

    while (true) {
        this->WriteToLogFile("Iteration number: " + std::to_string(iter_count), Log::debug);
        if(iter_count % 10 == 0) {
            this->WriteToLogFile("Iteration number: " + std::to_string(iter_count), Log::info);
        }

        /** SECTION MinCut Each Connected Component START **/
        this->WriteToLogFile(std::to_string(MincutOnly::to_be_mincut_clusters.size()) + " [connected components / clusters] to be mincut", Log::debug);
        before_mincut_number_of_clusters = MincutOnly::to_be_mincut_clusters.size();
        /* if a thread gets a cluster {-1}, then they know processing is done and they can stop working */
        for(int i = 0; i < this->num_processors; i ++) {
            MincutOnly::to_be_mincut_clusters.push({-1});
        }
        /* start the threads */
        std::vector<std::thread> thread_vector;
        for(int i = 0; i < this->num_processors; i ++) {
            thread_vector.push_back(std::thread(MincutOnly::MinCutWorker, &graph, this->connectedness_criterion));
        }
        /* get the result back from threads */
        /* the results from each thread gets stored in to_be_clustered_clusters */
        for(size_t thread_index = 0; thread_index < thread_vector.size(); thread_index ++) {
            thread_vector[thread_index].join();
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


    this->WriteToLogFile("Writing output to: " + this->output_file, Log::info);
    this->WriteClusterQueue(MincutOnly::done_being_mincut_clusters, &graph);
    igraph_destroy(&graph);
    return 0;
}
