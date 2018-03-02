#include "engpar_queue.h"
#include "engpar_bfsvisitfn.h"
#include <PCU.h>

#define __CL_ENABLE_EXCEPTIONS
#include <cl.hpp>
#include "util.hpp"
#include "engpar_opencl_config.h"


cl::Context* engpar_ocl_context;
cl::CommandQueue* engpar_ocl_queue;
cl::Device* engpar_ocl_device;

#ifdef ENGPAR_OPENCL_ALTERA
extern "C" void aocl_mmd_card_info(
    const char *name , int id, size_t sz,
    void *v, size_t *retsize);

namespace {
  float readboardpower(void) {
    float pwr;
    size_t retsize;
    aocl_mmd_card_info("aclnalla_pcie0", 9,
        sizeof(float),
        (void*)&pwr, &retsize);
    return pwr;
  }
  void monitorPower(cl::Event e) {
    const char* statusStrings[4] = {"complete","running","submitted","queued"};
    cl_int status;
    do {
      float during = readboardpower();
      e.getInfo(CL_EVENT_COMMAND_EXECUTION_STATUS, &status);
      printf("kernel %s, power (W) %f\n", statusStrings[status], during);
    } while( status != CL_COMPLETE );
    return;
  }
}
#else
namespace {
  void monitorPower(cl::Event) {
    return;
  }
}
#endif

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


  cl::Program* createProgram(std::string kernelFileName, int degree=0) {
    std::string ext = kernelFileName.substr(kernelFileName.find_last_of(".") + 1);
    fprintf(stderr, "extension %s\n", ext.c_str());
    cl::Program* program;
    cl_int err = 0;
    if( ext == "cl" ) {
      program =
        new cl::Program(*engpar_ocl_context, util::loadProgram(kernelFileName));
      try {
        if( degree == 4 )
          program->build("-DCSR_UNROLL -DSCG_UNROLL -Werror");
        else
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

  /** \brief check if the chunk degree is uniform
   *  \return 0 if nonuniform; o.w., the uniform degree
   **/
  int isUniformDegree(agi::PNgraph* pg, agi::etype t) {
    if( pg->isSellCSigma ) {
      int C = pg->chunk_size;
      agi::lid_t degree = (pg->degree_list[t][1] - pg->degree_list[t][0])/C;
      for(int i=1; i < pg->num_vtx_chunks; i++) {
        const agi::lid_t maxChunkDeg = (pg->degree_list[t][i+1] - pg->degree_list[t][i])/C;
        if( degree != maxChunkDeg )
          return 0;
      }
      return degree;
    } else {
      agi::lid_t degree = pg->degree_list[t][1] - pg->degree_list[t][0];
      for (agi::lid_t i=1; i < pg->num_local_verts; i++) {
        const agi::lid_t d = pg->degree_list[t][i+1] - pg->degree_list[t][i];
        if( degree != d )
          return 0;
      }
      return degree;
    }
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
    int degree = isUniformDegree(pg,t);
    printf("graph vtx->hyperedge degree %d\n", degree);
    cl::Program* program = createProgram(kernel,degree);

    cl::make_kernel
      <cl::Buffer,        //degreeList
       cl::Buffer,        //edgeList
       cl::Buffer,        //depth
       cl::Buffer,        //changes
       cl_int,            //num verts
       cl_int>            //bfs level
      bfsScgKernel(*program, "bfsScgKernel");

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
    agi::lid_t edgeListSize = pg->degree_list[t][pg->num_vtx_chunks];
    printf("host: number of vertex chunks %ld\n", pg->num_vtx_chunks);
    printf("host: edgeListSize %ld\n", edgeListSize);
    cl::Buffer* d_degreeList = copyToDevice<int>(
        pg->degree_list[t],
        pg->num_vtx_chunks+1,
        CL_MEM_READ_ONLY);
    cl::Buffer* d_edgeList = copyToDevice<int>(
        pg->edge_list[t],
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
    int h_changes;
    do {
      h_changes = 0;
      // run the kernel once for each chunk
      cl::Buffer* d_changes = copyToDevice<int>(
          &h_changes,
          1,
          CL_MEM_WRITE_ONLY);
      cl::Event e = bfsScgKernel(cl::EnqueueArgs(*engpar_ocl_queue, global, local),
          *d_degreeList, *d_edgeList, *d_depth, *d_changes, pg->num_local_verts, level);

      monitorPower(e);

      copyFromDevice<int>(d_changes, &h_changes, 1);
      printf("level %d changes %d\n", level, h_changes);
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

    return 0;
  }

  /*
    Pull based BFS using Pipelined OpenCL takes in :
      graph
      edge type
      first seed index into depth array
      starting depth
      the visit function
      inputs object
  */
  int bfsPullOpenclScgPipelined(agi::PNgraph* pg, agi::etype t,
      agi::lid_t start_seed, int start_depth, Inputs* in,
      std::string kernel) {
    int degree = isUniformDegree(pg,t);
    printf("graph vtx->hyperedge degree %d\n", degree);
    cl::Program* program = createProgram(kernel,degree);

    cl::make_kernel
      <cl::Buffer,        //degreeList
       cl::Buffer,        //edgeList
       cl::Buffer,        //depth
       cl_int,            //num chunks
       cl_int,            //chunk length
       cl_int,            //num verts
       cl_int>            //start depth
      bfsScgPipelinedKernel(*program, "bfsScgPipelinedKernel");

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
    agi::lid_t edgeListSize = pg->degree_list[t][pg->num_vtx_chunks];
    printf("host: number of vertex chunks %ld\n", pg->num_vtx_chunks);
    printf("host: edgeListSize %ld\n", edgeListSize);
    cl::Buffer* d_degreeList = copyToDevice<int>(
        pg->degree_list[t],
        pg->num_vtx_chunks+1,
        CL_MEM_READ_ONLY);
    cl::Buffer* d_edgeList = copyToDevice<int>(
        pg->edge_list[t],
        edgeListSize,
        CL_MEM_READ_ONLY);
    ////////
    // vertex depth array
    ////////
    cl::Buffer* d_depth = copyToDevice<int>(
        in->visited,
        pg->num_local_edges[t],
        CL_MEM_READ_WRITE);

    // single workitem kernel!
    cl::NDRange one(1);
    cl::Event e = bfsScgPipelinedKernel(cl::EnqueueArgs(*engpar_ocl_queue, one, one),
        *d_degreeList, *d_edgeList, *d_depth,
        pg->num_vtx_chunks, pg->chunk_size, pg->num_local_verts, start_depth);

    monitorPower(e);

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

    return 0;
  }

  int bfsPullOpenclCsr(agi::PNgraph* pg, agi::etype t,agi::lid_t start_seed,
               int start_depth, Inputs* in, std::string kernel) {
    int degree = isUniformDegree(pg,t);
    printf("graph vtx->hyperedge degree %d\n", degree);
    cl::Program* program = createProgram(kernel,degree);

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
               int start_depth, visitFn, Inputs* in, std::string kernel, bool isPipelined) {
    agi::PNgraph* pg = g->publicize();
    if( ! pg->isHyperGraph ) {
      fprintf(stderr, "ERROR: %s requires a hypergraph... exiting\n", __func__);
      exit(EXIT_FAILURE);
    }
    int err = 1;
    if( pg->isSellCSigma ) {
      if( isPipelined )
        err = bfsPullOpenclScgPipelined(pg,t,start_seed,start_depth,in,kernel);
      else
        err = bfsPullOpenclScg(pg,t,start_seed,start_depth,in,kernel);
    } else {
      err = bfsPullOpenclCsr(pg,t,start_seed,start_depth,in,kernel);
    }
    return err;
  }

}
