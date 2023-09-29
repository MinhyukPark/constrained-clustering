# Constrained clustering


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
