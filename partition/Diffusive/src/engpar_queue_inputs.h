#ifndef ENGPAR_QUEUE_INPUTS_H
#define ENGPAR_QUEUE_INPUTS_H

namespace engpar {
  class Inputs {
  public:
    Inputs() {
      seeds=NULL;
      numSeeds=0;
      visited=NULL;
      num_osets=0;
      num_sets=0;
      parents=NULL;
      set_size=NULL;
      labels=NULL;
    }
    ~Inputs() {
      if (seeds)
        delete [] seeds;
      if (visited)
        delete [] visited;
      if (parents) 
        delete [] parents;
      if (set_size)
        delete [] set_size;
      if (labels)
        delete [] labels;
    }
    agi::lid_t* seeds;
    agi::lid_t numSeeds;
    int* visited;

    int num_osets;//= numSeeds-start_seed;
    int num_sets; //= num_osets;
    int* parents; //= new int[num_sets];
    int* set_size;//= new int[num_sets];
    int* labels;  //= new int[pg->num_local_edges[t]];
  };
}
#endif
