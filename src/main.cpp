#include <iostream>

#include "argparse.h"
#include "constrained.h"
#include "library.h"
#include "mincut_global_cluster_repeat.h"
#include "cm.h"


int main(int argc, char* argv[]) {
    argparse::ArgumentParser main_program("constrained-clustering");
    argparse::ArgumentParser mincut_global_cluster_repeat("MincutGlobalClusterRepeat");
    mincut_global_cluster_repeat.add_description("Mincut and then Cluster Globally");

    argparse::ArgumentParser cm("CM");
    cm.add_description("CM");

    mincut_global_cluster_repeat.add_argument("--edgelist")
        .required()
        .help("Network edge-list file");
    mincut_global_cluster_repeat.add_argument("--algorithm")
        .help("Clustering algorithm to be used (leiden-cpm, leiden-mod, louvain)")
        .action([](const std::string& value) {
            static const std::vector<std::string> choices = {"leiden-cpm", "leiden-mod", "louvain"};
            if (std::find(choices.begin(), choices.end(), value) != choices.end()) {
                return value;
            }
            throw std::invalid_argument("--algorithm can only take in leiden-cpm, leiden-mod, or louvain.");
        });
    mincut_global_cluster_repeat.add_argument("--resolution")
        .default_value(double(0.01))
        .help("Resolution value for leiden-cpm. Only used if --algorithm is leiden-cpm")
        .scan<'f', double>();
    mincut_global_cluster_repeat.add_argument("--start-with-clustering")
        .default_value(false)
        .implicit_value(true)
        .help("Whether to start with a run of the clustering algorithm or stick with mincut starts");
    mincut_global_cluster_repeat.add_argument("--num-processors")
        .default_value(int(1))
        .help("Number of processors")
        .scan<'d', int>();
    mincut_global_cluster_repeat.add_argument("--output-file")
        .required()
        .help("Output clustering file");
    mincut_global_cluster_repeat.add_argument("--log-file")
        .required()
        .help("Output log file");
    mincut_global_cluster_repeat.add_argument("--log-level")
        .default_value(int(1))
        .help("Log level where 0 = silent, 1 = info, 2 = verbose")
        .scan<'d', int>();

    cm.add_argument("--edgelist")
        .required()
        .help("Network edge-list file");
    cm.add_argument("--algorithm")
        .help("Clustering algorithm to be used (leiden-cpm, leiden-mod, louvain)")
        .action([](const std::string& value) {
            static const std::vector<std::string> choices = {"leiden-cpm", "leiden-mod", "louvain"};
            if (std::find(choices.begin(), choices.end(), value) != choices.end()) {
                return value;
            }
            throw std::invalid_argument("--algorithm can only take in leiden-cpm, leiden-mod, or louvain.");
        });
    cm.add_argument("--resolution")
        .default_value(double(0.01))
        .help("Resolution value for leiden-cpm. Only used if --algorithm is leiden-cpm")
        .scan<'f', double>();
    cm.add_argument("--start-with-clustering")
        .default_value(false)
        .implicit_value(true)
        .help("Whether to start with a run of the clustering algorithm or stick with mincut starts");
    cm.add_argument("--num-processors")
        .default_value(int(1))
        .help("Number of processors")
        .scan<'d', int>();
    cm.add_argument("--output-file")
        .required()
        .help("Output clustering file");
    cm.add_argument("--log-file")
        .required()
        .help("Output log file");
    cm.add_argument("--log-level")
        .default_value(int(1))
        .help("Log level where 0 = silent, 1 = info, 2 = verbose")
        .scan<'d', int>();

    main_program.add_subparser(mincut_global_cluster_repeat);
    main_program.add_subparser(cm);
    try {
        main_program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << main_program;
        std::exit(1);
    }

    if(main_program.is_subcommand_used(mincut_global_cluster_repeat)) {
        std::string edgelist = mincut_global_cluster_repeat.get<std::string>("--edgelist");
        std::string algorithm = mincut_global_cluster_repeat.get<std::string>("--algorithm");
        double resolution = mincut_global_cluster_repeat.get<double>("--resolution");
        bool start_with_clustering = mincut_global_cluster_repeat.get<bool>("--start-with-clustering");
        int num_processors = mincut_global_cluster_repeat.get<int>("--num-processors");
        std::string output_file = mincut_global_cluster_repeat.get<std::string>("--output-file");
        std::string log_file = mincut_global_cluster_repeat.get<std::string>("--log-file");
        int log_level = mincut_global_cluster_repeat.get<int>("--log-level") - 1; // so that enum is cleaner
        ConstrainedClustering* mcgcr = new MinCutGlobalClusterRepeat(edgelist, algorithm, resolution, start_with_clustering, num_processors, output_file, log_file, log_level);
        random_functions::setSeed(0);
        mcgcr->main();
        delete mcgcr;
    } else if(main_program.is_subcommand_used(cm)) {
        std::string edgelist = cm.get<std::string>("--edgelist");
        std::string algorithm = cm.get<std::string>("--algorithm");
        double resolution = cm.get<double>("--resolution");
        bool start_with_clustering = cm.get<bool>("--start-with-clustering");
        int num_processors = cm.get<int>("--num-processors");
        std::string output_file = cm.get<std::string>("--output-file");
        std::string log_file = cm.get<std::string>("--log-file");
        int log_level = cm.get<int>("--log-level") - 1; // so that enum is cleaner
        ConstrainedClustering* cm = new CM(edgelist, algorithm, resolution, start_with_clustering, num_processors, output_file, log_file, log_level);
        random_functions::setSeed(0);
        cm->main();
        delete cm;
    }
}
