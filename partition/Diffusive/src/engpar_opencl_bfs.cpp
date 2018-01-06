#include "engpar_queue.h"
#include "engpar_bfsvisitfn.h"
#include <PCU.h>

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
  }

}
