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
      std::cout << "Usage: ./nbody [OPTIONS]" << std::endl << std::endl;
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
    uint64_t startTime, endTime;
    util::Timer timer;

    parseArguments(argc, argv);

    // Get list of devices
    std::vector<cl::Device> devices;
    getDeviceList(devices);

    // Check device index in range
    if (deviceIndex >= devices.size())
    {
      std::cout << "Invalid device index (try '--list')" << std::endl;
      return 1;
    }

    cl::Device device = devices[deviceIndex];

    std::string name = getDeviceName(device);
    std::cout << std::endl << "Using OpenCL device: " << name << std::endl;

    cl::Context context(device);
    cl::CommandQueue queue(context);

    cl::Program program(context, util::loadProgram("bfskernel.cl"));
    try
    {
      program.build();
    }
    catch (cl::Error error)
    {
      if (error.err() == CL_BUILD_PROGRAM_FAILURE)
      {
        std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
        std::cerr << log << std::endl;
      }
      throw(error);
    }
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
  if (!PCU_Comm_Self()) {
    for (unsigned int i=0;i<q->size();i++)
      printf("%d %ld\n",PCU_Comm_Self(),g->globalID((*q)[i]));
  }
  delete q;
  delete input;
  
  if (g->hasCoords()) {
    std::string filename = "graph";
    agi::writeVTK(g,filename.c_str());
  }

  agi::destroyGraph(g);

  PCU_Barrier();
  if (!PCU_Comm_Self()) 
    printf("All Tests Passed\n"); 

  EnGPar_Finalize();
  MPI_Finalize();

  return 0;
}
