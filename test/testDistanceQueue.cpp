#include <ngraph.h>
#include <PCU.h>
#include <engpar_support.h>
#include "buildGraphs.h"
#include "../partition/Diffusive/engpar_diffusive_input.h"
#include "../partition/Diffusive/src/engpar_queue.h"

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
#include "util.hpp"
#include "err_code.h"
#include "device_picker.hpp"

static cl_uint  deviceIndex   =      0;
cl::Context* engpar_ocl_context;
cl::CommandQueue* engpar_ocl_queue;
cl::Device* engpar_ocl_device;

void parseArguments(int argc, char *argv[])
{
  for (int i = 1; i < argc; i++)
  {
    if (!strcmp(argv[i], "--list"))
    {
      // Get list of devices
      std::vector<cl::Device> devices;
      getDeviceList(devices);

      // Print device names
      if (devices.size() == 0)
      {
        std::cout << "No devices found." << std::endl;
      }
      else
      {
        std::cout << std::endl;
        std::cout << "Devices:" << std::endl;
        for (unsigned i = 0; i < devices.size(); i++)
        {
          std::cout << i << ": " << getDeviceName(devices[i]) << std::endl;
        }
        std::cout << std::endl;
      }
      exit(0);
    }
    else if (!strcmp(argv[i], "--device"))
    {
      if (++i >= argc || !parseUInt(argv[i], &deviceIndex))
      {
        std::cout << "Invalid device index" << std::endl;
        exit(1);
      }
    }
    else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
    {
      std::cout << std::endl;
      std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  -h  --help               Print the message" << std::endl;
      std::cout << "      --list               List available devices" << std::endl;
      std::cout << "      --device     INDEX   Select device at INDEX" << std::endl;
      std::cout << std::endl;
      exit(0);
    }
    else
    {
      std::cout << "Unrecognized argument '" << argv[i] << "' (try '--help')"
                << std::endl;
      exit(1);
    }
  }
}


int main(int argc, char* argv[]) {
  MPI_Init(&argc,&argv);
  EnGPar_Initialize();

  try
  {
    parseArguments(argc, argv);

    // Get list of devices
    std::vector<cl::Device> devices;
    getDeviceList(devices);

    // Check device index in range
    if (deviceIndex >= devices.size()) {
      std::cout << "Invalid device index (try '--list')" << std::endl;
      return 1;
    }

    engpar_ocl_device = new cl::Device(devices[deviceIndex]);

    std::string name = getDeviceName(*engpar_ocl_device);
    std::cout << std::endl << "Using OpenCL device: " << name << std::endl;

    engpar_ocl_context = new cl::Context(*engpar_ocl_device);
    engpar_ocl_queue = new cl::CommandQueue(*engpar_ocl_context);

  }
  catch (cl::Error err)
  {
    std::cout << "Exception:" << std::endl
              << "ERROR: "
              << err.what()
              << "("
              << err_code(err.err())
              << ")"
              << std::endl;
  }

  agi::Ngraph* g;
  if (argc==1)
    if (PCU_Comm_Peers()==2)
      g = buildDisconnected2Graph();
    else
      g=buildHyperGraphLine();
  else {
    g = agi::createEmptyGraph();
    g->loadFromFile(argv[1]);
  }
  PCU_Barrier();

  engpar::Input* input = engpar::createDiffusiveInput(g,0);
  engpar::DiffusiveInput* inp = static_cast<engpar::DiffusiveInput*>(input);
  engpar::Queue* q = engpar::createDistanceQueue(inp);
  delete q;
  delete input;
  
  agi::destroyGraph(g);

  PCU_Barrier();
  if (!PCU_Comm_Self()) 
    printf("All Tests Passed\n"); 

  delete engpar_ocl_device;
  delete engpar_ocl_context;
  delete engpar_ocl_queue;

  EnGPar_Finalize();
  MPI_Finalize();

  return 0;
}
