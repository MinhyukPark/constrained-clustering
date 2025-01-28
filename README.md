# Constrained clustering


## Recommened CC command
CC returns the connected components of each cluster as its own cluster
```
./constrained_clustering MincutOnly --edgelist <tab separated edgelist network> --existing-clustering <input clustering> --num-processors <maximum allowed parallelism> --output-file <output file path> --log-file <log file path> --log-level 1 --connectedness-criterion 0
```

## Recommened WCC command
WCC returns the connected components of each cluster as its own cluster as long as the connected component is well-connected. Otherwise, mincuts are performed until either the cluster disintegrates or a well-connected cluster is found. Note that the command for WCC and CC are identical except the connectedness criterion.
```
./constrained_clustering MincutOnly --edgelist <tab separated edgelist network> --existing-clustering <input clustering> --num-processors <maximum allowed parallelism> --output-file <output file path> --log-file <log file path> --log-level 1 --connectedness-criterion 1
```


## Environment setup
The project uses gcc/11.2.0 and cmake/3.26.3 for now.

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
#### CC or WCC
```
$ constrained_clustering MincutOnly --help
Usage: MincutOnly [--help] [--version] --edgelist VAR --existing-clustering VAR [--num-processors VAR] --output-file VAR --log-file VAR [--connectedness-criterion VAR] [--log-level VAR]

Optional arguments:
  -h, --help                 shows help message and exits
  -v, --version              prints version information and exits
  --edgelist                 Network edge-list file [required]
  --existing-clustering      Existing clustering file [required]
  --num-processors           Number of processors [default: 1]
  --output-file              Output clustering file [required]
  --log-file                 Output log file [required]
  --connectedness-criterion  Log level where 0 = simple, 1 = well-connectedness [default: 0]
  --log-level                Log level where 0 = silent, 1 = info, 2 = verbose [default: 1]
```

#### CM
```
$ constrained_clustering CM --help
Usage: CM [--help] [--version] --edgelist VAR [--algorithm VAR] [--resolution VAR] [--start-with-clustering] [--existing-clustering VAR] [--num-processors VAR] --output-file VAR --log-file VAR [--log-level VAR]

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
