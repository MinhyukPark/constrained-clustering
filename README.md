# Constrained clustering


## MincutGlobalClusterRepeat
MincutGlobalClusterRepeat starts with a network and does rounds of the following four steps.
1. Get the connected components in the network and split each component into two subsets by its mincut
    1. Ensure that there are no edges that connect nodes in one subset to nodes in another subset. In other words, only keep edges that start and end within its own subset.
2. Run a clustering method on the entire network which now has fewer edges due to the previous removal process.
    1. Again, ensure that after the clustering method has run, only those edges that connect nodes within a cluster survive. Remove any edge that connect a node from one cluster to a node in another cluster.


This process repeats until we detect that after step 1 all clusters are well-connected and there are no mincuts to be made.


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
$ constrained_clustering MincutGlobalClusterRepeat --help
Usage: MincutGlobalClusterRepeat [--help] [--version] --edgelist VAR [--algorithm VAR] [--resolution VAR] [--num-processors VAR] --output-file VAR --log-file VAR [--log-level VAR]

Mincut

Optional arguments:
  -h, --help        shows help message and exits
  -v, --version     prints version information and exits
  --edgelist        Network edge-list file [required]
  --algorithm       Clustering algorithm to be used (leiden-cpm, leiden-mod, louvain)
  --resolution      Resolution value for leiden-cpm. Only used if --algorithm is leiden-cpm [default: 0.01]
  --num-processors  Number of processors [default: 1]
  --output-file     Output clustering file [required]
  --log-file        Output log file [required]
  --log-level       Log level where 0 = silent, 1 = info, 2 = verbose [default: 1]
```
