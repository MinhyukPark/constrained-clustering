#include "constrained.h"
#include <unistd.h>

std::map<int, std::vector<int>> ConstrainedClustering::ReverseMap(const std::map<int, int>& node_id_to_cluster_id_map) {
    std::map<int, std::vector<int>> reversed_map;
    for (auto const& [node_id, cluster_id] : node_id_to_cluster_id_map) {
        reversed_map[cluster_id].push_back(node_id);
    }
    return reversed_map;
}

int ConstrainedClustering::FindMaxClusterId(const std::map<int, std::vector<int>>& cluster_id_to_node_id_map) {
    bool is_first = true;
    int max_cluster_id = -1;
    for (auto const& [cluster_id, cluster_member_vec] : cluster_id_to_node_id_map) {
        if (is_first) {
            max_cluster_id = cluster_id;
            is_first = false;
        }
        max_cluster_id = std::max(max_cluster_id, cluster_id);
    }
    return max_cluster_id;
}

std::map<int, std::string> ConstrainedClustering::InvertMap(const std::map<std::string, int>& original_to_new_id_map) {
    std::map<int, std::string> new_to_original_id_map;
    for(auto const& [original_id, new_id] : original_to_new_id_map) {
        new_to_original_id_map[new_id] = original_id;
    }
    return new_to_original_id_map;
}

std::map<std::string, int> ConstrainedClustering::GetOriginalToNewIdMap(std::string edgelist) {
    std::map<std::string, int> original_to_new_id_map;
    int next_node_id = 0;

    if (is_binary_edgelist(edgelist)) {
        std::ifstream file(edgelist, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open binary edgelist: " + edgelist);
        }
        uint32_t num_edges;
        file.read(reinterpret_cast<char*>(&num_edges), sizeof(num_edges));

        for (uint32_t i = 0; i < num_edges; ++i) {
            int32_t source, target;
            file.read(reinterpret_cast<char*>(&source), sizeof(source));
            file.read(reinterpret_cast<char*>(&target), sizeof(target));

            std::string source_str = std::to_string(source);
            std::string target_str = std::to_string(target);

            if (!original_to_new_id_map.contains(source_str)) {
                original_to_new_id_map[source_str] = next_node_id++;
            }
            if (!original_to_new_id_map.contains(target_str)) {
                original_to_new_id_map[target_str] = next_node_id++;
            }
        }
        this->num_edges = static_cast<int>(num_edges);
        return original_to_new_id_map;
    }

    // Text path (fallback)
    char delimiter = get_delimiter(edgelist);
    std::ifstream edgelist_file(edgelist);
    std::string line;
    int line_no = 0;
    while(std::getline(edgelist_file, line)) {
        std::stringstream ss(line);
        std::string current_value;
        std::vector<std::string> current_line;
        while(std::getline(ss, current_value, delimiter)) {
            current_line.push_back(current_value);
        }
        if(line_no == 0) {
            line_no ++;
            continue;
        }
        std::string source = current_line[0];
        std::string target = current_line[1];
        if (!original_to_new_id_map.contains(source)) {
            original_to_new_id_map[source] = next_node_id;
            next_node_id ++;
        }
        if (!original_to_new_id_map.contains(target)) {
            original_to_new_id_map[target] = next_node_id;
            next_node_id ++;
        }
        line_no ++;
    }
    this->num_edges = line_no - 1;
    return original_to_new_id_map;
}

void ConstrainedClustering::LoadEdgesFromFile(igraph_t* graph, std::string edgelist, const std::map<std::string, int>& original_to_new_id_map) {
    igraph_add_vertices(graph, original_to_new_id_map.size(), NULL);

    if (is_binary_edgelist(edgelist)) {
        std::ifstream file(edgelist, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open binary edgelist: " + edgelist);
        }
        uint32_t num_edges;
        file.read(reinterpret_cast<char*>(&num_edges), sizeof(num_edges));

        igraph_vector_int_t edges;
        igraph_vector_int_init(&edges, num_edges * 2);
        int edge_index = 0;
        for (uint32_t i = 0; i < num_edges; ++i) {
            int32_t source, target;
            file.read(reinterpret_cast<char*>(&source), sizeof(source));
            file.read(reinterpret_cast<char*>(&target), sizeof(target));

            VECTOR(edges)[edge_index++] = original_to_new_id_map.at(std::to_string(source));
            VECTOR(edges)[edge_index++] = original_to_new_id_map.at(std::to_string(target));
        }
        igraph_add_edges(graph, &edges, NULL);
        igraph_vector_int_destroy(&edges);
        return;
    }

    // Text path (fallback)
    char delimiter = get_delimiter(edgelist);
    std::ifstream edgelist_file(edgelist);
    std::string line;
    igraph_vector_int_t edges;
    igraph_vector_int_init(&edges, this->num_edges * 2);
    int line_no = 0;
    int edge_index = 0;
    while(std::getline(edgelist_file, line)) {
        std::stringstream ss(line);
        std::string current_value;
        std::vector<std::string> current_line;
        while(std::getline(ss, current_value, delimiter)) {
            current_line.push_back(current_value);
        }
        if(line_no == 0) {
            line_no ++;
            continue;
        }
        std::string source = current_line[0];
        std::string target = current_line[1];
        VECTOR(edges)[edge_index] = original_to_new_id_map.at(source);
        edge_index ++;
        VECTOR(edges)[edge_index] = original_to_new_id_map.at(target);
        edge_index ++;
        line_no ++;

    }
    igraph_add_edges(graph, &edges, NULL);
    igraph_vector_int_destroy(&edges);
}

void ConstrainedClustering::WriteClusterHistory(const std::map<int, std::vector<int>>& parent_to_child_map) {
    std::ofstream history_output(this->history_file);
    for (auto const& [parent_id, children_vec] : parent_to_child_map) {
        history_output << parent_id << ":";
        for (size_t i = 0; i < children_vec.size(); i ++) {
            if (i != 0) {
                history_output << ",";
            }
            history_output << children_vec.at(i);
        }
        history_output << "\n";
    }
    history_output.close();
}

void ConstrainedClustering::WriteClusterQueue(std::queue<std::pair<std::vector<int>, int>>& cluster_queue, igraph_t* graph, const std::map<int, std::string>& new_to_original_id_map) {
    std::ofstream clustering_output(this->output_file);
    this->WriteToLogFile("final clusters:", Log::debug);
    clustering_output << "node_id,cluster_id" << '\n';
    while(!cluster_queue.empty()) {
        std::pair<std::vector<int>, int> current_front = cluster_queue.front();
        std::vector<int> current_cluster = current_front.first;
        int current_cluster_id = current_front.second;
        cluster_queue.pop();
        this->WriteToLogFile("new cluster", Log::debug);
        for(size_t i = 0; i < current_cluster.size(); i ++) {
            this->WriteToLogFile(std::to_string(current_cluster[i]), Log::debug);
            /* clustering_output << VAS(graph, "name", current_cluster[i]) << "," << current_cluster_id << '\n'; */
            clustering_output << new_to_original_id_map.at(current_cluster[i]) << "," << current_cluster_id << '\n';
        }
    }
    clustering_output.close();
}

void ConstrainedClustering::WriteClusterQueue(std::queue<std::vector<int>>& cluster_queue, igraph_t* graph, const std::map<int, std::string>& new_to_original_id_map) {
    std::ofstream clustering_output(this->output_file);
    int current_cluster_id = 0;
    this->WriteToLogFile("final clusters:", Log::debug);
    clustering_output << "node_id,cluster_id" << '\n';
    while(!cluster_queue.empty()) {
        std::vector<int> current_cluster = cluster_queue.front();
        cluster_queue.pop();
        this->WriteToLogFile("new cluster", Log::debug);
        for(size_t i = 0; i < current_cluster.size(); i ++) {
            this->WriteToLogFile(std::to_string(current_cluster[i]), Log::debug);
            /* clustering_output << VAS(graph, "name", current_cluster[i]) << "," << current_cluster_id << '\n'; */
            clustering_output << new_to_original_id_map.at(current_cluster[i]) << "," << current_cluster_id << '\n';
        }
        current_cluster_id ++;
    }
    clustering_output.close();
}

void ConstrainedClustering::WritePartitionMap(std::map<int, int>& final_partition) {
    std::ofstream clustering_output(this->output_file);
    for(auto const& [node_id, cluster_id]: final_partition) {
        clustering_output << node_id << "," << cluster_id << '\n';
    }
    clustering_output.close();
}

int ConstrainedClustering::WriteToLogFile(std::string message, Log message_type) {
    if(this->log_level >= message_type) {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        std::string log_message_prefix;
        if(message_type == Log::info) {
            log_message_prefix = "[INFO]";
        } else if(message_type == Log::debug) {
            log_message_prefix = "[DEBUG]";
        } else if(message_type == Log::error) {
            log_message_prefix = "[ERROR]";
        }
        auto days_elapsed = std::chrono::duration_cast<std::chrono::days>(now - this->start_time);
        auto hours_elapsed = std::chrono::duration_cast<std::chrono::hours>(now - this->start_time - days_elapsed);
        auto minutes_elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - this->start_time - days_elapsed - hours_elapsed);
        auto seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - this->start_time - days_elapsed - hours_elapsed - minutes_elapsed);
        auto total_seconds_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - this->start_time);
        log_message_prefix += "[";
        log_message_prefix += std::to_string(days_elapsed.count());
        log_message_prefix += "-";
        log_message_prefix += std::to_string(hours_elapsed.count());
        log_message_prefix += ":";
        log_message_prefix += std::to_string(minutes_elapsed.count());
        log_message_prefix += ":";
        log_message_prefix += std::to_string(seconds_elapsed.count());
        log_message_prefix += "]";

        log_message_prefix += "(t=";
        log_message_prefix += std::to_string(total_seconds_elapsed.count());
        log_message_prefix += "s)";
        this->log_file_handle << log_message_prefix << " " << message << '\n';

        if(this->num_calls_to_log_write % 10 == 0) {
            std::flush(this->log_file_handle);
        }
        this->num_calls_to_log_write ++;
    }
    return 0;
}

void ConstrainedClustering::WriteYieldCluster(const igraph_t* graph,
                                              const std::vector<int>& cluster_nodes,
                                              const std::map<int, std::string>& new_to_original_id_map) {
    int yield_id = this->next_yield_id++;

    // Create yield directory on-demand (only when a yield actually happens)
    std::filesystem::create_directories(this->yield_dir);

    // Extract induced subgraph (same pattern used throughout the CC codebase)
    igraph_vector_int_t nodes_to_keep;
    igraph_vector_int_t new_id_to_old_id_vec;
    igraph_vector_int_init(&nodes_to_keep, cluster_nodes.size());
    igraph_vector_int_init(&new_id_to_old_id_vec, cluster_nodes.size());
    for (size_t i = 0; i < cluster_nodes.size(); i++) {
        VECTOR(nodes_to_keep)[i] = cluster_nodes[i];
    }
    igraph_t induced_subgraph;
    igraph_induced_subgraph_map(graph, &induced_subgraph,
        igraph_vss_vector(&nodes_to_keep), IGRAPH_SUBGRAPH_CREATE_FROM_SCRATCH,
        NULL, &new_id_to_old_id_vec);

    // Iterate edges of the induced subgraph, translate to original IDs
    std::vector<std::pair<int32_t, int32_t>> edges;
    igraph_eit_t eit;
    igraph_eit_create(&induced_subgraph, igraph_ess_all(IGRAPH_EDGEORDER_ID), &eit);
    for (; !IGRAPH_EIT_END(eit); IGRAPH_EIT_NEXT(eit)) {
        igraph_integer_t edge_id = IGRAPH_EIT_GET(eit);
        int from_local = IGRAPH_FROM(&induced_subgraph, edge_id);
        int to_local = IGRAPH_TO(&induced_subgraph, edge_id);
        int from_global = VECTOR(new_id_to_old_id_vec)[from_local];
        int to_global = VECTOR(new_id_to_old_id_vec)[to_local];
        int32_t orig_src = std::stoi(new_to_original_id_map.at(from_global));
        int32_t orig_tgt = std::stoi(new_to_original_id_map.at(to_global));
        edges.emplace_back(orig_src, orig_tgt);
    }
    igraph_eit_destroy(&eit);
    igraph_vector_int_destroy(&nodes_to_keep);
    igraph_vector_int_destroy(&new_id_to_old_id_vec);
    igraph_destroy(&induced_subgraph);

    // Write .bedgelist (binary)
    {
        std::string path = this->yield_dir + "/" + std::to_string(yield_id) + ".bedgelist";
        std::ofstream out(path, std::ios::binary);
        uint32_t num_edges = static_cast<uint32_t>(edges.size());
        out.write(reinterpret_cast<const char*>(&num_edges), sizeof(num_edges));
        for (const auto& [src, tgt] : edges) {
            out.write(reinterpret_cast<const char*>(&src), sizeof(src));
            out.write(reinterpret_cast<const char*>(&tgt), sizeof(tgt));
        }
    }

    // Write .bcluster (binary — all nodes map to one cluster)
    {
        std::string path = this->yield_dir + "/" + std::to_string(yield_id) + ".bcluster";
        std::ofstream out(path, std::ios::binary);
        uint32_t num_entries = static_cast<uint32_t>(cluster_nodes.size());
        out.write(reinterpret_cast<const char*>(&num_entries), sizeof(num_entries));
        for (int node : cluster_nodes) {
            int32_t orig_node = std::stoi(new_to_original_id_map.at(node));
            int32_t cid = static_cast<int32_t>(yield_id);
            out.write(reinterpret_cast<const char*>(&orig_node), sizeof(orig_node));
            out.write(reinterpret_cast<const char*>(&cid), sizeof(cid));
        }
    }

    this->yield_records.push_back({yield_id, (int)cluster_nodes.size(), (int)edges.size()});

    // Notify parent process via pipe (if connected)
    if (this->yield_fd >= 0) {
        int32_t record[3] = {
            static_cast<int32_t>(yield_id),
            static_cast<int32_t>(cluster_nodes.size()),
            static_cast<int32_t>(edges.size())
        };
        // Write atomically (12 bytes < PIPE_BUF, guaranteed atomic on POSIX)
        ::write(this->yield_fd, record, sizeof(record));
    }

    this->WriteToLogFile("Yielded sub-cluster " + std::to_string(yield_id) +
                         " (nodes=" + std::to_string(cluster_nodes.size()) +
                         ", edges=" + std::to_string(edges.size()) + ")", Log::info);
}

void ConstrainedClustering::WriteYieldSummary() {
    if (this->yield_records.empty()) return;

    std::string path = this->yield_dir + "/yield_summary.csv";
    std::ofstream out(path);
    out << "yield_id,node_count,edge_count\n";
    for (const auto& rec : this->yield_records) {
        out << rec.yield_id << "," << rec.node_count << "," << rec.edge_count << "\n";
    }
}

void ConstrainedClustering::InitializeConnectednessCriterion() {
    size_t log_position = this->connectedness_criterion.find("log_");
    size_t n_caret_position = this->connectedness_criterion.find("n^");
    double connectedness_criterion_c = 1;
    double connectedness_criterion_x = 1;
    double pre_computed_log = -1;
    ConnectednessCriterion current_connectedness_criterion = ConnectednessCriterion::Simple;
    if (log_position != std::string::npos) {
        // is Clog_x(n)
        current_connectedness_criterion = ConnectednessCriterion::Logarithimic;
        if (log_position == 0) {
            connectedness_criterion_c = 1;
        } else {
            connectedness_criterion_c = std::stod(this->connectedness_criterion.substr(0, log_position));
        }
        size_t open_parenthesis_position = this->connectedness_criterion.find("(", log_position + 4);
        connectedness_criterion_x = std::stod(this->connectedness_criterion.substr(log_position + 4, open_parenthesis_position));
    } else if (n_caret_position != std::string::npos) {
        // is cN^x
        current_connectedness_criterion = ConnectednessCriterion::Exponential;
        if (n_caret_position == 0) {
            connectedness_criterion_c = 1;
        } else {
            connectedness_criterion_c = std::stod(this->connectedness_criterion.substr(0, n_caret_position));
        }
        connectedness_criterion_x = std::stod(this->connectedness_criterion.substr(n_caret_position + 2));
    } else if (this->connectedness_criterion != "0") {
        // wasn't log or exponent so if it isn't 0 then it's an error
        // exit
        this->WriteToLogFile("Colud not parse connectedness_criterion" , Log::error);
        this->WriteToLogFile("Accepted forms are Clog_x(n), Cn^x, or 0 where C and x are numbers" , Log::error);
    }
    if (current_connectedness_criterion == ConnectednessCriterion::Simple) {
        this->WriteToLogFile("Running with mode (mincut of each cluster has to be greater than 0)" , Log::info);
    } else if (current_connectedness_criterion == ConnectednessCriterion::Logarithimic) {
        this->WriteToLogFile("Running with mode (mincut of each cluster has to be greater than " + std::to_string(connectedness_criterion_c) + " times log base " + std::to_string(connectedness_criterion_x) + "of n" , Log::info);
        pre_computed_log = connectedness_criterion_c / std::log(connectedness_criterion_x);
    } else if (current_connectedness_criterion == ConnectednessCriterion::Exponential) {
        this->WriteToLogFile("Running with mode (mincut of each cluster has to be greater than " + std::to_string(connectedness_criterion_c) + " times n to the power of " + std::to_string(connectedness_criterion_x), Log::info);
    } else {
        // should not possible to reach
        exit(1);
    }
    this->connectedness_criterion_c = connectedness_criterion_c;
    this->connectedness_criterion_x = connectedness_criterion_x;
    this->pre_computed_log = pre_computed_log;
    this->current_connectedness_criterion = current_connectedness_criterion;
}
