#include "constrained.h"

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
    char delimiter = get_delimiter(edgelist);
    std::ifstream edgelist_file(edgelist);
    std::string line;
    int line_no = 0;
    int next_node_id = 0;
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
    /* igraph_setup(); */
    igraph_add_vertices(graph, original_to_new_id_map.size(), NULL);
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
        /* igraph_add_edge(graph, original_to_new_id_map.at(source), original_to_new_id_map.at(target)); */
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
        this->WriteToLogFile("Accepted forms are Clog_x(n), Cn^x, or 0 where C and x are integers" , Log::error);
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
