#include "engpar_queue.h"
#include "engpar_bfsvisitfn.h"
#include <PCU.h>

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
#include "util.hpp"

cl::Context* engpar_ocl_context;
cl::CommandQueue* engpar_ocl_queue;
cl::Device* engpar_ocl_device;

namespace {
  // copy buffer from the host to the device
  template <typename T>
  cl::Buffer* copyToDevice(T* in, int numVals, int mode) {
    cl::Buffer* d_buff = new cl::Buffer(*engpar_ocl_context, mode, numVals*sizeof(T));
    cl::copy(*engpar_ocl_queue, in, in+numVals, *d_buff);
    return d_buff;
  }

  // copy buffer from the device to the host
  template <typename T>
  void copyFromDevice(cl::Buffer* d_buff, T* h_buff, int numVals) {
    cl::copy(*engpar_ocl_queue, *d_buff, h_buff, h_buff+numVals);
  }

  cl::Program* createProgram(std::string kernelFileName) {
    cl::Program* program =
      new cl::Program(*engpar_ocl_context, util::loadProgram(kernelFileName));
    try {
      program->build("-cl-opt-disable -Werror");
      std::string opts = program->getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(*engpar_ocl_device);
      std::string log = program->getBuildInfo<CL_PROGRAM_BUILD_LOG>(*engpar_ocl_device);
      std::cerr << "----OpenCL Build Options----\n" << opts << std::endl;
      std::cerr << "----OpenCL Build Log--------\n" << log << std::endl;
      std::cerr << "----------------------------" << std::endl;
    } catch (cl::Error error) {
      std::cerr << "OpenCL Build Error... exiting" << std::endl;
      throw(error);
    }
    return program;
  }
}

namespace engpar {
  /*
    Pull based BFS using OpenCL takes in :
      graph
      edge type
      first seed index into depth array
      starting depth
      the visit function
      inputs object
  */
  int bfs_pull_OpenCL(agi::Ngraph* g, agi::etype t,agi::lid_t start_seed,
               int start_depth, visitFn visit, Inputs* in) {
    std::cout << "OpenCL creating program." << std::endl << std::endl;
    cl::Program* program = createProgram("bfskernel.cl");

    std::cout << "OpenCL calling make_kernel." << std::endl << std::endl;
    cl::make_kernel
      <cl::Buffer,        //degreeList
       cl::Buffer,        //edgeList
       cl_long,           //numEdges
       cl::Buffer,        //depth
       cl::Buffer,        //seeds
       cl_long,           //numSeeds
       cl_int,            //startDepth
       cl::LocalSpaceArg> //localWork
      bfsPullKernel(*program, "bfskernel");

    // initialize the visited/depth array
    agi::PNgraph* pg = g->publicize();
    for (agi::lid_t i=start_seed;i<in->numSeeds;i++) 
      in->visited[in->seeds[i]] = start_depth;

    ////////
    // copy the graph CSRs to the device
    ////////
    printf("host: numEdges numSeeds startDepth %ld %ld %d\n",
           pg->num_local_edges[t], in->numSeeds, start_depth);
    // vert-to-nets
    cl::Buffer* d_degreeList = copyToDevice<agi::lid_t>(
        pg->degree_list[t],
        pg->num_local_verts+1,
        CL_MEM_READ_ONLY);
    cl::Buffer* d_edgeList = copyToDevice<agi::lid_t>(
        pg->edge_list[t],
        pg->num_local_edges[t],
        CL_MEM_READ_ONLY);
    ////////
    // vertex depth array
    ////////
    cl::Buffer* d_depth = copyToDevice<int>(
        in->visited,
        pg->num_local_edges[t],
        CL_MEM_READ_WRITE);
    cl::Buffer* d_seeds = copyToDevice<agi::lid_t>(
        in->seeds,
        pg->num_local_edges[t],
        CL_MEM_READ_ONLY);
    cl::LocalSpaceArg localWork = cl::Local(sizeof(int)*sizeof(pg->num_local_verts));

    std::cout << "OpenCL initialization complete." << std::endl << std::endl;
    cl::NDRange global(pg->num_local_verts);

    std::cout << "OpenCL kernel queued" << std::endl << std::endl;
    bfsPullKernel(cl::EnqueueArgs(*engpar_ocl_queue, global),
        *d_degreeList, *d_edgeList, pg->num_local_edges[t],
        *d_depth, *d_seeds, in->numSeeds, start_depth, localWork);

    copyFromDevice<int>(d_depth, in->visited, pg->num_local_edges[t]);

    // TODO: reconstruct seeds array from depth array
    //  the serial implementation computes the seeds array on the fly, but that
    //  is hard to do in a parallel implementation without using atomics
    // The seeds array contains contiguous groups of edges with the same depth
    // in ascending order (first group is depth 1).

    delete d_degreeList;
    delete d_edgeList;
    delete d_depth;
    delete d_seeds;
    delete program;
  }

}
