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
    std::string ext = kernelFileName.substr(kernelFileName.find_last_of(".") + 1);
    fprintf(stderr, "extension %s\n", ext.c_str());
    cl::Program* program;
    cl_int err = 0;
    if( ext == "cl" ) {
      program =
        new cl::Program(*engpar_ocl_context, util::loadProgram(kernelFileName));
      try {
        program->build("-Werror");
      } catch (cl::Error error) {
        std::cerr << "OpenCL Build Error..." << std::endl;
        err = 1;
      }
      printBuildLogAndOpts(program);
    } else if ( ext == "aocx" ) {
      std::vector<cl::Device> devices;
      devices.push_back(*engpar_ocl_device);
      std::string bytes = util::loadProgram(kernelFileName);
      cl::Program::Binaries bin;
      typedef std::pair<const void*, ::size_t> binPair;
      bin.push_back( binPair(bytes.c_str(),bytes.length()) );
      std::vector<cl_int> binStatus;
      program =
        new cl::Program(*engpar_ocl_context, devices, bin, &binStatus, &err);

      assert(binStatus.size() == 1);
      assert(binStatus[0] == CL_SUCCESS);
    } else {
      if(!PCU_Comm_Self())
        fprintf(stderr, "uknown kernel file extension... exiting\n");
      err = 1;
    }
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
  int bfsPullOpenclScg(agi::PNgraph* pg, agi::etype t,agi::lid_t start_seed,
               int start_depth, Inputs* in, std::string kernel) {
    cl::Program* program = createProgram(kernel);

    cl::make_kernel
      <cl::Buffer,        //degreeList
       cl::Buffer,        //edgeList
       cl::Buffer,        //depth
       cl::Buffer,        //front size
       cl_int,            //num verts
       cl_int>            //bfs level
      bfsScgKernel(*program, "bfsScgKernel");

    // copy the arrays of longs to int arrays
    int* dl = new int[pg->num_vtx_chunks+1];
    for(int i=0; i < pg->num_vtx_chunks+1; i++)
      dl[i] = pg->degree_list[t][i];
    agi::lid_t edgeListSize = pg->degree_list[t][pg->num_vtx_chunks];
    int* el = new int[edgeListSize];
    for(int i=0; i < edgeListSize; i++)
      el[i] = pg->edge_list[t][i];

    double t0=PCU_Time();
    // initialize the visited/depth array
    for (agi::lid_t i=start_seed;i<in->numSeeds;i++) 
      in->visited[in->seeds[i]] = start_depth;

    printf("host: numVtx numEdges numPins numSeeds startDepth "
           "%ld %ld %ld %ld %d\n",
           pg->num_local_verts,
           pg->num_local_edges[t],
           pg->num_local_pins[t],
           in->numSeeds,
           start_depth);
    ////////
    // copy the graph CSRs to the device
    ////////
    // vert-to-nets
    printf("host: number of vertex chunks %ld\n", pg->num_vtx_chunks);
    printf("host: edgeListSize %ld\n", edgeListSize);
    cl::Buffer* d_degreeList = copyToDevice<int>(
        dl,
        pg->num_vtx_chunks+1,
        CL_MEM_READ_ONLY);
    cl::Buffer* d_edgeList = copyToDevice<int>(
        el,
        edgeListSize,
        CL_MEM_READ_ONLY);
    ////////
    // vertex depth array
    ////////
    cl::Buffer* d_depth = copyToDevice<int>(
        in->visited,
        pg->num_local_edges[t],
        CL_MEM_READ_WRITE);

    cl::NDRange global(pg->num_vtx_chunks*pg->chunk_size);
    cl::NDRange local(pg->chunk_size);

    int maxLevel=1000;
    int level=start_depth;
    int h_frontSize;
    do {
      h_frontSize = 0;
      // run the kernel once for each chunk
      cl::Buffer* d_frontSize = copyToDevice<int>(
          &h_frontSize,
          1,
          CL_MEM_WRITE_ONLY);
      bfsScgKernel(cl::EnqueueArgs(*engpar_ocl_queue, global, local),
          *d_degreeList, *d_edgeList, *d_depth, *d_frontSize, pg->num_local_verts, level);
      copyFromDevice<int>(d_frontSize, &h_frontSize, 1);
      printf("level %d frontSize %d\n", level, h_frontSize);
      level++;
    } while(h_frontSize && level < maxLevel);

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
    if(!PCU_Comm_Self())
      printf("opencl bfs time (s) %f\n", PCU_Time()-t0);

    delete [] dl;
    delete [] el;

    return 0;
  }

  int bfsPullOpenclCsr(agi::PNgraph* pg, agi::etype t,agi::lid_t start_seed,
               int start_depth, Inputs* in, std::string kernel) {
    cl::Program* program = createProgram(kernel);

    cl::make_kernel
      <cl::Buffer,        //degreeList
       cl::Buffer,        //edgeList
       cl::Buffer,        //depth
       cl::Buffer,        //update flag
       cl_int>            //bfs level
      bfsCsrKernel(*program, "bfsCsrKernel");

    double t0=PCU_Time();
    // initialize the visited/depth array
    for (agi::lid_t i=start_seed;i<in->numSeeds;i++) 
      in->visited[in->seeds[i]] = start_depth;

    printf("host: numVtx numEdges numPins numSeeds startDepth "
           "%ld %ld %ld %ld %d\n",
           pg->num_local_verts,
           pg->num_local_edges[t],
           pg->num_local_pins[t],
           in->numSeeds,
           start_depth);
    ////////
    // copy the graph CSRs to the device
    ////////
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

    cl::NDRange global(pg->num_local_verts);

    int maxLevel=1000;
    int level=start_depth;
    char h_changes;
    do { 
      h_changes = false;
      // run the kernel once for each chunk
      cl::Buffer* d_changes = copyToDevice<char>(
          &h_changes,
          1,
          CL_MEM_WRITE_ONLY);
      bfsCsrKernel(cl::EnqueueArgs(*engpar_ocl_queue, global),
          *d_degreeList, *d_edgeList, *d_depth, *d_changes, level);
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
    if(!PCU_Comm_Self())
      printf("opencl bfs time (s) %f\n", PCU_Time()-t0);
  }

  int bfs_pull_OpenCL(agi::Ngraph* g, agi::etype t,agi::lid_t start_seed,
               int start_depth, visitFn, Inputs* in, std::string kernel) {
    agi::PNgraph* pg = g->publicize();
    if( ! pg->isHyperGraph ) {
      fprintf(stderr, "ERROR: %s requires a hypergraph... exiting\n", __func__);
      exit(EXIT_FAILURE);
    }
    int err = 1;
    if( pg->isSellCSigma ) {
      err = bfsPullOpenclScg(pg,t,start_seed,start_depth,in,kernel);
    } else {
      err = bfsPullOpenclCsr(pg,t,start_seed,start_depth,in,kernel);
    }
    return err;
  }

}
