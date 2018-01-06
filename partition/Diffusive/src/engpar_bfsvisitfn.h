#ifndef ENGPAR_BFSVISITFN_H
#define ENGPAR_BFSVISITFN_H

#include "engpar_queue_inputs.h"

namespace engpar {
  //Visit function takes in the input, the source edge and destination edge
  typedef bool (*visitFn)(Inputs*,agi::lid_t,agi::lid_t);
}

#endif
