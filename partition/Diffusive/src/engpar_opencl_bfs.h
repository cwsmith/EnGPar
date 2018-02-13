#ifndef ENGPAR_OPENCL_BFS_H
#define ENGPAR_OPENCL_BFS_H

#include <string>
#include "engpar_queue_inputs.h"
namespace engpar {
  int bfs_pull_OpenCL(agi::Ngraph* g, agi::etype t,agi::lid_t start_seed,
             int start_depth, visitFn visit, Inputs* in, std::string kernel);
}
#endif
