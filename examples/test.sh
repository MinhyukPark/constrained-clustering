#/bin/bash

./constrained_clustering MincutGlobalClusterRepeat --edgelist ./examples/connected_components.tsv --algorithm leiden-cpm --resolution 0.01 --num-processors 1 --output-file ./output.file --log-file ./output.log --log-level 2
