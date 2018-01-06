#include "engpar_queue.h"
#include "engpar_bfsvisitfn.h"
#include <PCU.h>

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
#include "util.hpp"
#include "err_code.h"

extern cl::Context* engpar_ocl_context;
extern cl::CommandQueue* engpar_ocl_queue;
extern cl::Device* engpar_ocl_device;

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
  }

}
