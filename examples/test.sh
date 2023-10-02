#/bin/bash

# ./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/connected_components.tsv --algorithm leiden-cpm --resolution 0.01 --num-processors 1 --output-file ./output_clusters.tsv --log-file .//output.log --log-level 2
# ./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/mincut_then_leiden.tsv --algorithm leiden-cpm --resolution 0.01 --num-processors 1 --output-file ./output_clusters.tsv --log-file .//output.log --log-level 2
# ./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/mincut_then_leiden.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 5 --output-file ./output_clusters.tsv --log-file .//output.log --log-level 2
./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/mincut_then_leiden.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 5 --output-file ./output_clusters.tsv --log-file ./output.log --log-level 1
