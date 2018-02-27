#include <ngraph.h>
#include <PCU.h>
#include <engpar_support.h>
#include "buildGraphs.h"
#include "../partition/Diffusive/engpar_diffusive_input.h"
#include "../partition/Diffusive/src/engpar_queue.h"

#define __CL_ENABLE_EXCEPTIONS
#include <cl.hpp>
#include "util.hpp"
#include "device_picker.hpp"

extern cl::Context* engpar_ocl_context;
extern cl::CommandQueue* engpar_ocl_queue;
extern cl::Device* engpar_ocl_device;


void parseDriverArguments(int argc, char *argv[],
    cl_uint *deviceIndex, int* bfsmode, bool* pipelined,
    int* chunkSize, std::string& graphFileName, std::string& kernelFileName) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--list")) {
      // Get list of devices
      std::vector<cl::Device> devices;
      unsigned numDevices = getDeviceList(devices);

      // Print device names
      if (numDevices == 0) {
        std::cout << "No devices found.\n";
      } else {
        std::cout << "\nDevices:\n";
        for (unsigned int i = 0; i < numDevices; i++) {
          std::cout << i << ": " << getDeviceName(devices[i]) << "\n";
        }
        std::cout << "\n";
      }
      exit(0);
    } else if (!strcmp(argv[i], "--chunkSize")) {
      if (++i >= argc) {
        std::cout << "Invalid chunkSize\n";
        exit(1);
      }
      *chunkSize = atoi(argv[i]);
    } else if (!strcmp(argv[i], "--kernel")) {
      if (++i >= argc) {
        std::cout << "Invalid kernel\n";
        exit(1);
      }
      kernelFileName = std::string(argv[i]);
    } else if (!strcmp(argv[i], "--device")) {
      if (++i >= argc || !parseUInt(argv[i], deviceIndex)) {
        std::cout << "Invalid device index\n";
        exit(1);
      }
    } else if (!strcmp(argv[i], "--graph")) {
      if (++i >= argc) {
        std::cout << "Invalid graph\n";
        exit(1);
      }
      graphFileName = std::string(argv[i]);
    } else if (!strcmp(argv[i], "--bfsmode")) {
      if (++i >= argc) {
        std::cout << "Invalid bfsmode\n";
        exit(1);
      }
      *bfsmode = atoi(argv[i]);
    } else if (!strcmp(argv[i], "--pipelined")) {
      *pipelined = true;
    } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
      std::cout << "\n";
      std::cout << "      --bfsmode     [0|1|2|3]   0:push, 1:pull, 2:csrOpenCL 3:scgOpenCL" << std::endl;
      std::cout << "      --pipelined               enable the pipelined opencl kernel" << std::endl;
      std::cout << "      --chunkSize   [1:N]       size of SCG chunks, only applies to bfsmode=3" << std::endl;
      std::cout << "      --graph       <pathToGraphFile.bgd>" << std::endl;
      std::cout << "      --list               List available devices\n";
      std::cout << "      --device     INDEX   Select device at INDEX\n";
      std::cout << "      --kernelBinary  <pathToKernelBinary.aocx> Specify the path to the percompiled kernel\n";
      std::cout << "\n";
      exit(0);
    }
  }
}

int main(int argc, char* argv[]) {
  MPI_Init(&argc,&argv);
  EnGPar_Initialize();

  int bfsmode = 0;
  int chunkSize = 1;
  bool pipelined = false;
  agi::Ngraph* g;
  std::string graphFileName("");
  std::string kernelFileName("");
  try
  {
    cl_uint deviceIndex = 0;
    parseDriverArguments(argc,argv,&deviceIndex,&bfsmode,&pipelined,&chunkSize,graphFileName,kernelFileName);

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


  printf("graph file: \'%s\'\n", graphFileName.c_str());
  if ( graphFileName == "" )
    if (PCU_Comm_Peers()==2)
      g = buildDisconnected2Graph();
    else
      g=buildHyperGraphLine();
  else {
    g = agi::createEmptyGraph();
    g->loadFromFile(graphFileName.c_str());
  }
  PCU_Barrier();

  engpar::Input* input = engpar::createDiffusiveInput(g,0);
  engpar::DiffusiveInput* inp = static_cast<engpar::DiffusiveInput*>(input);

  inp->chunkSize = chunkSize;
  inp->kernel = kernelFileName;
  inp->bfsPush = inp->bfsPull = inp->bfsCsrOpenCL = inp->bfsScgOpenCL = false;
  inp->isPipelined = pipelined;
  if (bfsmode == 0)
    inp->bfsPush = true;
  else if (bfsmode == 1)
    inp->bfsPull = true;
  else if (bfsmode == 2)
    inp->bfsCsrOpenCL = true;
  else if (bfsmode == 3)
    inp->bfsScgOpenCL = true;
  else {
    printf("invalid bfsmode specified... exiting\n");
    exit(EXIT_FAILURE);
  }
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
