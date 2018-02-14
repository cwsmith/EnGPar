# SCOREC EnGPar

EnGPar is a set of C/C++ libraries for partitioning relational data structures using a parallel graph structure to represent the data.

### Repository Layout

* AGI: APIs for the graph structure, N-Graph.
* interfaces: Extensions of the N-Graph for certain data structures.
* Zoltan: Partitioning methods utilizing Zoltan
* partition: Diffusive load balancing routines
* test: A set of tests to utilize the features of EnGPar

### Getting started

* Dependencies: CMake, MPI, PCU (can be grabbed from [SCOREC Core](https://github.com/SCOREC/core))
* Configuration: two example configuration files are provided in the EnGPar directory minimal_config.sh and config.sh. The first has the minimum requirements to setup a directory while the second lists the specific flags to turn on and off the various portions of EnGPar.
* Testing: After building the code, the test directory will have several tests and standalone executables that use the
different functionalities of EnGPar.

### End-user Build

Edit `minimal_config.sh` and set the path to the PUMI install directory via the
`SCOREC_PREFIX` variable.

```
mkdir build
../minimal_config.sh
make
```

### Developer Build

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

### Build with OpenCL Support

To enable a build that uses the OpenCL BFS kernel start by passing `-DENABLE_OPENCL=On` to cmake.  This in turn calls [find_package(opencl)](https://cmake.org/cmake/help/v3.10/module/FindOpenCL.html).  If this fails then you'll need to locate the directory where `libOpenCL.so` and the C header `CL/cl.h` are, and set `CMAKE_PREFIX_PATH` to include those paths.

Next, we will need the C++ host API wrapper header `cl.hpp`.  If the header is not found, then pass 
`-DOPENCL_HPP_DIR=/path/to/directory/containing/cl.hpp`.

Some system specific notes are below on where to find the required libraries and headers.

## blockade (SCOREC)

Blockade has 

## OSX 

My macbook has an Intel i5 processor with integrated graphics.  Interestingly, opencl just seems to work out of the box.  Maybe installing xcode provides this.

Apple only supports work group sizes of 1 on the CPU.  This is a known issue that has persisted for several years. (Simon Smith from University of Bristol provided this info).

## Ruth (JLSE)

The following environment file, `ruth.env`, is used for building and running on the [Arria 10](https://www.altera.com/products/fpga/arria-series/arria-10/overview.html) fpga.

```
#altera/intel SDK and 'aoc' compiler for the kernel
source  /soft/fpga/altera/pro/16.0.2.222/aoc-env.sh

#mpi
export PATH=$PATH:/soft/libraries/mpi/mpich-gcc_4.4.7-3.1.2/bin

#cmake
export PATH=/home/cwsmith/ruth-software/cmake-3.10.2/install/bin:$PATH

#opencl paths
export CMAKE_PREFIX_PATH=\
$ALTERAOCLSDKROOT/host/linux64/lib:\
$ALTERAOCLSDKROOT/host/include:\
$ALTERAOCLSDKROOT/board/nalla_pcie/linux64/lib/:\
$ALTERAOCLSDKROOT/host/linux64/lib/:\
$CMAKE_PREFIX_PATH

#pumi - needed for PCU
export PUMI_INSTALL_DIR=/home/cwsmith/build-core-ruth/install
```

The kernel is built using the `aoc` compiler
```
aoc /path/to/engpar/partition/Diffusion/src/bfskernel.cl
```
this will take more than five hours and consume around 1.7GB.

Compiling the host code requires linking against three additional libraries provided by Altera/Intel: `nalla_pcie_mmd`, `alteracl`, and `elf`.  Passing the flag `-DENABLE_OPENCL_Altera=On` to cmake will search for these libraries in the paths listed in `CMAKE_PREFIX_PATH` set in the environment `ruth.env` file above.

### Website

http://scorec.github.io/EnGPar/

### Contact Developers
* Gerrett Diamond <diamog@rpi.edu>
