#include <iostream>

#include "argparse.h"
#include "constrained.h"
#include "library.h"
#include "mincut_only.h"
#include "cm.h"


int main(int argc, char* argv[]) {
    argparse::ArgumentParser main_program("constrained-clustering");
    argparse::ArgumentParser cm("CM");
    cm.add_description("CM");

    argparse::ArgumentParser mincut_only("MincutOnly");
    mincut_only.add_description("WCC");

    cm.add_argument("--edgelist")
        .required()
        .help("Network edge-list file");
    cm.add_argument("--algorithm")
        .help("Clustering algorithm to be used (leiden-cpm, leiden-mod, louvain, infomap)")
        .action([](const std::string& value) {
            static const std::vector<std::string> choices = {"leiden-cpm", "leiden-mod", "louvain", "infomap"};
            if (std::find(choices.begin(), choices.end(), value) != choices.end()) {
                return value;
            }
            throw std::invalid_argument("--algorithm can only take in leiden-cpm, leiden-mod, louvain, or infomap.");
        });
    cm.add_argument("--clustering-parameter")
        .default_value(double(0.01))
        .help("Clustering parameter e.g., 0.01 for Leiden-CPM")
        .scan<'f', double>();
    cm.add_argument("--existing-clustering")
        .default_value("")
        .help("Existing clustering file");
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
    cm.add_argument("--history-file")
        .required()
        .help("Output history file");
    cm.add_argument("--log-level")
        .default_value(int(1))
        .help("Log level where 0 = silent, 1 = info, 2 = verbose")
        .scan<'d', int>();
    cm.add_argument("--connectedness-criterion")
        .default_value("1log_10(n)")
        .help("String in the form of Clog_x(n) or Cn^x for well-connectedness");
    cm.add_argument("--prune")
        .default_value(false) // default false, implicit true
        .implicit_value(true) // default false, implicit true
        .help("Whether to prune nodes using mincuts");
    cm.add_argument("--mincut-type")
        .default_value("cactus")
        .help("Mincut type used (cactus or noi)");
    /* END comment out cm */

    mincut_only.add_argument("--edgelist")
        .required()
        .help("Network edge-list file");
    mincut_only.add_argument("--existing-clustering")
        .required()
        .help("Existing clustering file");
    mincut_only.add_argument("--num-processors")
        .default_value(int(1))
        .help("Number of processors")
        .scan<'d', int>();
    mincut_only.add_argument("--output-file")
        .required()
        .help("Output clustering file");
    mincut_only.add_argument("--log-file")
        .required()
        .help("Output log file");
    mincut_only.add_argument("--log-level")
        .default_value(int(1))
        .help("Log level where 0 = silent, 1 = info, 2 = verbose")
        .scan<'d', int>();
    mincut_only.add_argument("--connectedness-criterion")
        .default_value("1log_10(n)")
        .help("String where CC = 0, and otherwise would be in the form of Clog_x(n) or Cn^x for well-connectedness");
    mincut_only.add_argument("--mincut-type")
        .default_value("cactus")
        .help("Mincut type used (cactus or noi)");
/*         The two functional forms would be: */
/* F(n) = C log_x(n), where C and x are parameters specified by the user (our default is C=1 and x=10) */
/* G(n) = C n^x, where C and x are parameters specified by the user (here, presumably 0<x<2). Note that x=1 makes it linear. */

    main_program.add_subparser(cm);
    main_program.add_subparser(mincut_only);

    try {
        main_program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << main_program;
        std::exit(1);
    }

    if(main_program.is_subcommand_used(cm)) {
        std::string edgelist = cm.get<std::string>("--edgelist");
        std::string algorithm = cm.get<std::string>("--algorithm");
        double clustering_parameter = cm.get<double>("--clustering-parameter");
        std::string existing_clustering = cm.get<std::string>("--existing-clustering");
        int num_processors = cm.get<int>("--num-processors");
        std::string output_file = cm.get<std::string>("--output-file");
        std::string log_file = cm.get<std::string>("--log-file");
        std::string history_file = cm.get<std::string>("--history-file");
        int log_level = cm.get<int>("--log-level") - 1; // so that enum is cleaner
        std::string connectedness_criterion = cm.get<std::string>("--connectedness-criterion");
        bool prune = false;
        if (cm["--prune"] == true) {
            prune = true;
            std::cerr << "pruning" << std::endl;
        }
        std::string mincut_type = cm.get<std::string>("--mincut-type");
        ConstrainedClustering* connectivity_modifier = new CM(edgelist, algorithm, clustering_parameter, existing_clustering, num_processors, output_file, log_file, history_file, log_level, connectedness_criterion, prune, mincut_type);
        random_functions::setSeed(0);
        connectivity_modifier->main();
        delete connectivity_modifier;
    } else if(main_program.is_subcommand_used(mincut_only)) {
        std::string edgelist = mincut_only.get<std::string>("--edgelist");
        std::string existing_clustering = mincut_only.get<std::string>("--existing-clustering");
        int num_processors = mincut_only.get<int>("--num-processors");
        std::string output_file = mincut_only.get<std::string>("--output-file");
        std::string log_file = mincut_only.get<std::string>("--log-file");
        int log_level = mincut_only.get<int>("--log-level") - 1; // so that enum is cleaner
        std::string connectedness_criterion = mincut_only.get<std::string>("--connectedness-criterion");
        std::string mincut_type = mincut_only.get<std::string>("--mincut-type");
        ConstrainedClustering* mincut_only = new MincutOnly(edgelist, existing_clustering, num_processors, output_file, log_file, log_level, connectedness_criterion, mincut_type);
        random_functions::setSeed(0);
        mincut_only->main();
        delete mincut_only;
    }
}
