#include <iostream>

#include "argparse.h"
#include "constrained.h"
#include "library.h"


int main(int argc, char* argv[]) {
    argparse::ArgumentParser main_program("constrained-clustering");
    argparse::ArgumentParser mincut_global_cluster_repeat("MincutGlobalClusterRepeat");
    mincut_global_cluster_repeat.add_description("Mincut ");

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

    main_program.add_subparser(mincut_global_cluster_repeat);
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
        int num_processors = mincut_global_cluster_repeat.get<int>("--num-processors");
        std::string output_file = mincut_global_cluster_repeat.get<std::string>("--output-file");
        std::string log_file = mincut_global_cluster_repeat.get<std::string>("--log-file");
        int log_level = mincut_global_cluster_repeat.get<int>("--log-level");
        ConstrainedClustering* mcgcr = new MinCutGlobalClusterRepeat(edgelist, algorithm, resolution, num_processors, output_file, log_file, log_level);
        mcgcr->main();
        delete mcgcr;
    }
}
