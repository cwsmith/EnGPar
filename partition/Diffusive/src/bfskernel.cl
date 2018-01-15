typedef long lid_t;

void printDimensions(void);

void printDimensions(void) {
  uint self = get_global_id(0);
  uint globalSize[3] = {0, 0, 0};
  uint localSize[3] = {0, 0, 0};
  uint dim = get_work_dim();
  for(uint d=0; d<dim; d++) {
     localSize[d] = get_local_size(d);
     globalSize[d] = get_global_size(d);
  }
  if( !self ) {
    printf("device: localsize globalsize ");
    for(uint d=0; d<dim; d++)
        printf(" <%d> %d %d ", d, localSize[d], globalSize[d]);
    printf("\n");
  }
}


kernel void bfskernel(global long* degreeList,
                      global long* edgeList,
                      const long numEdges,
                      const long numPins,
                      global int* depth,
                      global long* seeds,
                      const long numSeeds,
                      const int startDepth,
                      local int* localWork,
                      global char* isDone)
{
  uint self = get_global_id(0);

  printDimensions();
  isDone[self] = 1;  // global array
}
