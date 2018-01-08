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
    cl::make_kernel<cl::Buffer, cl::Buffer> bfsPullKernel(program, "bfskernel");

    // device buffer test
    const int buffsize = 128;
    cl::Buffer d_verts = cl::Buffer(*engpar_ocl_context,
                                    CL_MEM_READ_WRITE,
                                    buffsize*sizeof(int));
    int* h_verts = (int*) malloc(buffsize*sizeof(int));
    for(int i=0; i<buffsize; i++)
      h_verts[i] = i;
    cl::copy(*engpar_ocl_queue, h_verts, h_verts+buffsize, d_verts);

    ////////
    // copy the graph CSRs to the device
    ////////
    agi::PNgraph* pg = g->publicize();
    // vert-to-nets
    cl::Buffer* d_degreeList = copyToDevice<agi::lid_t>(
        pg->degree_list[t],
        pg->num_local_verts+1,
        CL_MEM_READ_ONLY);
    //cl::Buffer d_degreeList = cl::Buffer(*engpar_ocl_context,
    //                                CL_MEM_READ,
    //                                (pg->num_local_verts[t]+1)*sizeof(int));

    //cl::Buffer d_edgeList = cl::Buffer(*engpar_ocl_context,
    //                                CL_MEM_READ,
    //                                pg->num_local_edges[t]*sizeof(int));

    // net-to-verts
    // COMING SOON

    std::cout << "OpenCL initialization complete." << std::endl << std::endl;
    cl::NDRange global(128);
    std::cout << "OpenCL kernel queued" << std::endl << std::endl;
    bfsPullKernel(cl::EnqueueArgs(*engpar_ocl_queue, global), d_verts, *d_degreeList);
    std::cout << "OpenCL start copying device buffere to host" << std::endl << std::endl;
    cl::copy(*engpar_ocl_queue, d_verts, h_verts, h_verts+buffsize);
    std::cout << "OpenCL done copying device buffere to host" << std::endl << std::endl;
    for(int i=0; i<buffsize; i++)
      assert(h_verts[i] = i+1);
    delete [] h_verts;
    delete d_degreeList;
  }

}
