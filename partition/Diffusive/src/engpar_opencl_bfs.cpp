#include "engpar_queue.h"
#include "engpar_bfsvisitfn.h"
#include <PCU.h>

#define __CL_ENABLE_EXCEPTIONS
#include <cl.hpp>
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

  void printBuildLogAndOpts(cl::Program* program) {
    std::string opts = program->getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(*engpar_ocl_device);
    std::string log = program->getBuildInfo<CL_PROGRAM_BUILD_LOG>(*engpar_ocl_device);
    std::cerr << "----OpenCL Build Options----\n" << opts << std::endl;
    std::cerr << "----OpenCL Build Log--------\n" << log << std::endl;
    std::cerr << "----------------------------" << std::endl;
  }

  cl::Program* createProgram(std::string kernelFileName) {
    cl::Program* program =
      new cl::Program(*engpar_ocl_context, util::loadProgram(kernelFileName));

    int err = 0; 
    try { 
      program->build("-cl-opt-disable -Werror");
    } catch (cl::Error error) {
      std::cerr << "OpenCL Build Error..." << std::endl;
      err = 1;
    }
    printBuildLogAndOpts(program);
    if ( err ) exit(EXIT_FAILURE);
    return program;
  }

}

namespace engpar {

  void printInputs(agi::PNgraph* pg, Inputs* in, agi::etype t) {
    printf("\n\n----------------------------------------------\n");
    printf("host: degreeList ");
    for(long i = 0; i < pg->num_local_verts; i++)
      printf("%ld ", pg->degree_list[t][i]);
    printf("\n");
    printf("host: edgeList ");
    for(long i = 0; i < pg->num_local_pins[t]; i++)
      printf("%ld ", pg->edge_list[t][i]);
    printf("\n");
    printf("host: depth ");
    for(int i = 0; i < pg->num_local_edges[t]; i++)
      printf("%d ", in->visited[i]);
    printf("\n");
    printf("host: seeds ");
    for(long i = 0; i < pg->num_local_edges[t]; i++)
      printf("%ld ", in->seeds[i]);
    printf("\n");
    printf("----------------------------------------------\n\n");
  }

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
               int start_depth, visitFn, Inputs* in) {
    std::cout << "OpenCL creating program." << std::endl << std::endl;
    cl::Program* program = createProgram("bfskernel.cl");

    std::cout << "OpenCL calling make_kernel." << std::endl << std::endl;
    cl::make_kernel
      <cl::Buffer,        //degreeList
       cl::Buffer,        //edgeList
       cl_long,           //numPins
       cl_long,           //numEdges
       cl::Buffer,        //depth
       cl::Buffer>        //update flag
      bfsPullKernel(*program, "bfskernel");

    // initialize the visited/depth array
    agi::PNgraph* pg = g->publicize();
    for (agi::lid_t i=start_seed;i<in->numSeeds;i++) 
      in->visited[in->seeds[i]] = start_depth;

    printInputs(pg,in,t);

    ////////
    // copy the graph CSRs to the device
    ////////
    printf("host: numEdges numPins numSeeds startDepth %ld %ld %ld %d\n",
           pg->num_local_edges[t], pg->num_local_pins[t], in->numSeeds, start_depth);
    // vert-to-nets
    cl::Buffer* d_degreeList = copyToDevice<agi::lid_t>(
        pg->degree_list[t],
        pg->num_local_verts+1,
        CL_MEM_READ_ONLY);
    cl::Buffer* d_edgeList = copyToDevice<agi::lid_t>(
        pg->edge_list[t],
        pg->num_local_pins[t],
        CL_MEM_READ_ONLY);
    ////////
    // vertex depth array
    ////////
    cl::Buffer* d_depth = copyToDevice<int>(
        in->visited,
        pg->num_local_edges[t],
        CL_MEM_READ_WRITE);

    std::cout << "OpenCL initialization complete." << std::endl << std::endl;
    cl::NDRange global(pg->num_local_verts);

    int maxLevel=1000;
    int level=0;
    char h_changes;
    do { 
      h_changes = false;
      cl::Buffer* d_changes = copyToDevice<char>(
          &h_changes,
          1,
          CL_MEM_WRITE_ONLY);
      bfsPullKernel(cl::EnqueueArgs(*engpar_ocl_queue, global),
          *d_degreeList, *d_edgeList, pg->num_local_edges[t],
          pg->num_local_pins[t], *d_depth, *d_changes);
      copyFromDevice<char>(d_changes, &h_changes, 1);
      level++;
    } while(h_changes && level < maxLevel);

    copyFromDevice<int>(d_depth, in->visited, pg->num_local_edges[t]);

    // TODO: reconstruct seeds array from depth array
    //  the serial implementation computes the seeds array on the fly, but that
    //  is hard to do in a parallel implementation without using atomics
    // The seeds array contains contiguous groups of edges with the same depth
    // in ascending order (first group is depth 1).

    delete d_degreeList;
    delete d_edgeList;
    delete d_depth;
    delete program;
  }

}
