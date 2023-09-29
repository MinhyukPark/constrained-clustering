#!/bin/sh
rm ./constrained_clustering
rm -r build
mkdir build
cd build
cmake ..
cp compile_commands.json ../
make -j 1
ln -s $(readlink -f ./bin/constrained_clustering) ../constrained_clustering
