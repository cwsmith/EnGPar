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
  template <typename T>
  cl::Buffer* copyToDevice(T* in, int numVals, int mode) {
    cl::Buffer* d_buff = new cl::Buffer(*engpar_ocl_context, mode, numVals*sizeof(T));
    cl::copy(*engpar_ocl_queue, in, in+numVals, *d_buff);
    return d_buff;
  }
}

namespace engpar {
  /*
    Pull based BFS using OpenCL takes in :
      graph
      edge type
      first seed index into visited array
      starting depth
      the visit function
      inputs object
  */
  int bfs_pull_OpenCL(agi::Ngraph* g, agi::etype t,agi::lid_t start_seed,
               int start_depth, visitFn visit, Inputs* in) {
    std::cout << "OpenCL creating program." << std::endl << std::endl;
    cl::Program program(*engpar_ocl_context, util::loadProgram("bfskernel.cl"));
    try
    {
      program.build();
    }
    catch (cl::Error error)
    {
      if (error.err() == CL_BUILD_PROGRAM_FAILURE)
      {
        std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(*engpar_ocl_device);
        std::cerr << log << std::endl;
      }
      throw(error);
    }

    std::cout << "OpenCL calling make_kernel." << std::endl << std::endl;
    cl::make_kernel
      <cl::Buffer, cl::Buffer, cl::Buffer, cl::Buffer, cl_long, cl_long, cl_long>
      bfsPullKernel(program, "bfskernel");

    ////////
    // copy the graph CSRs to the device
    ////////
    agi::PNgraph* pg = g->publicize();
    printf("host: numVerts numEdges numPins %ld %ld %ld\n",
           pg->num_local_verts, pg->num_local_edges[t], pg->num_local_pins[t]);
    // vert-to-nets
    cl::Buffer* d_degreeList = copyToDevice<agi::lid_t>(
        pg->degree_list[t],
        pg->num_local_verts+1,
        CL_MEM_READ_ONLY);
    cl::Buffer* d_edgeList = copyToDevice<agi::lid_t>(
        pg->edge_list[t],
        pg->num_local_edges[t],
        CL_MEM_READ_ONLY);
    // net-to-verts
    cl::Buffer* d_pinDegreeList = copyToDevice<agi::lid_t>(
        pg->pin_degree_list[t],
        pg->num_local_edges[t]+1,
        CL_MEM_READ_ONLY);
    cl::Buffer* d_pinList = copyToDevice<agi::lid_t>(
        pg->pin_list[t],
        pg->num_local_pins[t],
        CL_MEM_READ_ONLY);

    std::cout << "OpenCL initialization complete." << std::endl << std::endl;
    cl::NDRange global(pg->num_local_edges[t]);

    std::cout << "OpenCL kernel queued" << std::endl << std::endl;
    bfsPullKernel(cl::EnqueueArgs(*engpar_ocl_queue, global),
        *d_degreeList, *d_edgeList, *d_pinDegreeList, *d_pinList,
        pg->num_local_verts, pg->num_local_edges[t], pg->num_local_pins[t]);

    delete d_degreeList;
    delete d_edgeList;
    delete d_pinDegreeList;
    delete d_pinList;
  }

}
