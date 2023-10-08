#/bin/bash

# ./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/connected_components.tsv --algorithm leiden-cpm --resolution 0.01 --num-processors 1 --output-file ./output_clusters.tsv --log-file .//output.log --log-level 2
# ./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/mincut_then_leiden.tsv --algorithm leiden-cpm --resolution 0.01 --num-processors 1 --output-file ./output_clusters.tsv --log-file .//output.log --log-level 2
# ./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/mincut_then_leiden.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 5 --output-file ./output_clusters.tsv --log-file .//output.log --log-level 2
# ./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/mincut_then_leiden.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 5 --output-file ./output_clusters.tsv --log-file ./output.log --log-level 1
# ./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/mincut_then_leiden.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 5 --output-file ./output_clusters.tsv --log-file ./output.log --log-level 1 #--start-with-clustering
# ./constrained_clustering CM --edgelist ./examples/cm_test.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 5 --output-file ./output_clusters.tsv --log-file ./output.log --log-level 2
# ./constrained_clustering CM --edgelist ./examples/cm_test.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 1 --output-file ./output_clusters.tsv --log-file ./output.log --log-level 2
# ./constrained_clustering CM --edgelist ./examples/cm_test.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 1 --output-file ./output_clusters.tsv --log-file ./output.log --log-level 2 --start-with-clustering
# ./constrained_clustering CM --edgelist ./examples/connected_components.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 1 --output-file ./output_clusters.tsv --log-file ./output.log --log-level 2 --start-with-clustering
# ./constrained_clustering CM --edgelist ./examples/connected_components.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 1 --output-file ./output_clusters.tsv --log-file ./output.log --log-level 2
./constrained_clustering CM --edgelist ./examples/connected_components_trivial_mincuts.tsv --algorithm leiden-cpm --resolution 0.2 --num-processors 1 --output-file ./output_clusters.tsv --log-file ./output.log --log-level 2
