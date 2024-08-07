# Constrained clustering


## Recommened CM command
CM starts with a clustering on the entire graph then processes each cluster one by one. For each cluster, the size of a mincut is computed and determined whether it fits the connectivity criterion or not. This is by default log base 10 of n where n is the size of the cluster. If the size of a mincut is greater than the threshold, then the cluster is considered well-connected. Otherwise, the cluster is split at the mincut and both sides of the mincut get clustered by the input clustering algorithm.
```
./constrained_clustering CM --edgelist <tab separated edgelist network> --algorithm <choose from leiden-cpm, leiden-mod, louvain> --resolution <only supply when algorithm is leiden-cpm. Floating point value> --start-with-clustering --num-processors <maximum allowed parallelism> --output-file <output file path> --log-file <log file path> --log-level 1
```



## Environment setup
The project uses gcc/11.2.0 and cmake/3.26.3 for now. On a module based cluster system, one can simply do the following commands to get the right environment for building and running.
```
module load gcc/11.2.0
module load cmake/3.26.3
```

## How to build
A one time setup is needed with the [setup.sh](setup.sh) script. This script builds the igraph and libleidenalg libraries.
```
./setup.sh
```

Once the one time setup is complete, all one has to do is run the script provided in [easy_build_and_compile.sh](easy_build_and_compile.sh).
```
./easy_build_and_compile.sh
```

### How to run the subcommands
```
$ constrained_clustering CM --help
Usage: CM [--help] [--version] --edgelist VAR [--algorithm VAR] [--resolution VAR] [--start-with-clustering] [--existing-clustering VAR] [--num-processors VAR] --output-file VAR --log-file VAR [--log-level VAR]

CM

Optional arguments:
  -h, --help               shows help message and exits
  -v, --version            prints version information and exits
  --edgelist               Network edge-list file [required]
  --algorithm              Clustering algorithm to be used (leiden-cpm, leiden-mod, louvain)
  --resolution             Resolution value for leiden-cpm. Only used if --algorithm is leiden-cpm [default: 0.01]
  --start-with-clustering  Whether to start with a run of the clustering algorithm or stick with mincut starts
  --existing-clustering    Existing clustering file [default: ""]
  --num-processors         Number of processors [default: 1]
  --output-file            Output clustering file [required]
  --log-file               Output log file [required]
  --log-level              Log level where 0 = silent, 1 = info, 2 = verbose [default: 1]
```
