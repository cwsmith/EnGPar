# SCOREC EnGPar #

EnGPar is a set of C/C++ libraries for partitioning relational data structures using a parallel graph structure to represent the data.

### Repository Layout ###

* AGI: APIs for the graph structure, N-Graph.
* interfaces: Extensions of the N-Graph for certain data structures.
* Zoltan: Partitioning methods utilizing Zoltan
* partition: Diffusive load balancing routines
* test: A set of tests to utilize the features of EnGPar

### Getting started ###

* Dependencies: CMake, MPI, PCU (can be grabbed from [SCOREC Core](https://github.com/SCOREC/core))
* Configuration: two example configuration files are provided in the EnGPar directory minimal_config.sh and config.sh. The first has the minimum requirements to setup a directory while the second lists the specific flags to turn on and off the various portions of EnGPar.
* Testing: After building the code, the test directory will have several tests and standalone executables that use the
different functionalities of EnGPar.
* [Calling EnGPar from your application](https://github.com/SCOREC/EnGPar/wiki/Getting-Started-with-EnGPar)

### End-user Build ###

Edit `minimal_config.sh` and set the path to the PUMI install directory via the
`SCOREC_PREFIX` variable.

```
mkdir build
../minimal_config.sh
make
```

### Developer Build ###

Setup the test mesh and graph repos via submodules
```
git submodule init
git submodule update
```

Edit `config.sh` and set the path to the PUMI install directory via the
`SCOREC_PREFIX` variable.

```
mkdir build
../config.sh
make
ctest
```

### Build with OpenCL Support ###

A hello world and nbody example from here:
http://www.iwocl.org/iwocl-2017/advanced-hands-on-opencl-tutorial/
runs on turtle and blockade.

My working repo is here:
https://bitbucket.org/c_smith/iwocl_2016_test

## blockade

```
cd /users/cwsmith/develop/openclTutorial/iwocl_2016_test/exercises/DeviceInfo
make
```

## OSX 

My macbook has an Intel i5 processor with integrated graphics.  Interestingly, opencl just seems to work out of the box.  Maybe installing xcode provides this.

Apple only supports work group sizes of 1 on the CPU.  This is a known issue that has persisted for several years. (Simon Smith from University of Bristol provided this info).

## Ruth (JLSE)

### Website ### 

http://scorec.github.io/EnGPar/

### Contact Developers ###
* Gerrett Diamond <diamog@rpi.edu>
