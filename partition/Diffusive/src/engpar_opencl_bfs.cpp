#include "engpar_queue.h"
#include "engpar_bfsvisitfn.h"
#include <PCU.h>

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
#include "util.hpp"

cl::Context* engpar_ocl_context;
cl::CommandQueue* engpar_ocl_queue;
cl::Device* engpar_ocl_device;

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
    cl::make_kernel<cl::Buffer> bfsPullKernel(program, "bfskernel");

    const int buffsize = 128;
    // device buffer
    cl::Buffer d_verts = cl::Buffer(*engpar_ocl_context,
                                    CL_MEM_READ_WRITE,
                                    buffsize*sizeof(int));

    int* h_verts = (int*) malloc(buffsize*sizeof(int));
    for(int i=0; i<buffsize; i++)
      h_verts[i] = i;

    cl::copy(*engpar_ocl_queue, h_verts, h_verts+buffsize, d_verts);

    std::cout << "OpenCL initialization complete." << std::endl << std::endl;
    cl::NDRange global(64);
    std::cout << "OpenCL kernel queued" << std::endl << std::endl;
    bfsPullKernel(cl::EnqueueArgs(*engpar_ocl_queue, global),d_verts);
    std::cout << "OpenCL start copying device buffere to host" << std::endl << std::endl;
    cl::copy(*engpar_ocl_queue, d_verts, h_verts, h_verts+buffsize);
    std::cout << "OpenCL done copying device buffere to host" << std::endl << std::endl;
  }

}
