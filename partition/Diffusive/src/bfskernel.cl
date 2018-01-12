typedef long lid_t;

int reductionSum(int in, local int* partial_sums);
int depth_visit(global int* depth, const long source, const long dest);
void assignEdges(int numEdges, int* first, int* last);
void printInputs(global long* degreeList,
        global long* edgeList,
        const long numEdges,
        global int* depth,
        global long* seeds,
        const long numSeeds,
        const int startDepth);

// Matthew Scarpino, "OpenCL in Action", Listing 10.2, 2012
int reductionSum(int in, local int* partial_sums) {
   int lid = get_local_id(0);
   int group_size = get_local_size(0);

   partial_sums[lid] = in;
   barrier(CLK_LOCAL_MEM_FENCE);

   for(int i = group_size/2; i>0; i >>= 1) {
      if(lid < i) {
         partial_sums[lid] += partial_sums[lid + i];
      }
      barrier(CLK_LOCAL_MEM_FENCE);
   }

   return partial_sums[0];
}

//Visit function for first traversal
int depth_visit(global int* depth, const long source, const long dest) {
    printf("device: depth_visit depth[dest] %d depth[source] %d\n",
            depth[dest], depth[source]);
    if (depth[dest]==-1) {
       depth[dest] = depth[source]+1;
       return 1;
    } else {
       return 0;
    }
}

void assignEdges(int numEdges, int* first, int* last) {
  const int lid = get_local_id(0);
  const int globSize = get_local_size(0);
  int edgesPerWorkItem = 0;
  if( numEdges < globSize )
      edgesPerWorkItem = 1;
  else
      edgesPerWorkItem = numEdges/globSize;

  *first = self*edgesPerWorkItem;
  *last = *first+edgesPerWorkItem;
  
  if( *first >= numEdges ) {
      *first = *last = -1;
  }

  if( *last >= numEdges )
      *last = numEdges;
}

void printInputs(global long* degreeList,
        global long* edgeList,
        const long numEdges,
        global int* depth,
        global long* seeds,
        const long numSeeds,
        const int startDepth) {
  uint self = get_global_id(0);
  if( !self ) {
    printf("\n\n----------------------------------------------\n");
    printf("device: numEdges numSeeds startDepth %ld %ld %d\n",
            numEdges, numSeeds, startDepth);
    printf("device: degreeList ");
    for(size_t i = 0; i < get_global_size(0); i++)
        printf("%ld ", degreeList[i]);
    printf("\n");
    printf("device: edgeList ");
    for(long i = 0; i < numEdges; i++)
        printf("%ld ", edgeList[i]);
    printf("\n");
    printf("device: depth ");
    for(int i = 0; i < numEdges; i++)
        printf("%d ", depth[i]);
    printf("\n");
    printf("device: seeds ");
    for(long i = 0; i < numEdges; i++)
        printf("%ld ", seeds[i]);
    printf("\n");
    printf("----------------------------------------------\n\n");
  }
  barrier(CLK_LOCAL_MEM_FENCE|CLK_GLOBAL_MEM_FENCE);
}


kernel void bfskernel(global long* degreeList,
                      global long* edgeList,
                      const long numEdges,
                      global int* depth,
                      global long* seeds,
                      const long numSeeds,
                      const int startDepth,
                      local int* localWork)
{
  uint self = get_global_id(0);
  uint dim = get_work_dim();
  uint globalSize[3] = {0, 0, 0};
  uint localSize[3] = {0, 0, 0};
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
  //printInputs(degreeList, edgeList, numEdges,
  //        depth, seeds, numSeeds, startDepth);

  int f, l;
  assignEdges(numEdges, &f, &l);
  const int first = f;
  const int last = l;
  printf("device: %d firstEdge lastEdge %d %d\n", self, first, last);

  int num_updates = 0;
  int level = 0;
  do {
    //printf("device: id %d level %d firstEdge lastEdge %d %d\n", self, level, first, last);
    num_updates = 0;
    int source = -1;
    for (lid_t j = degreeList[self]; j < degreeList[self+1]; j++){
      lid_t edge = edgeList[j];
      printf("device: found edge id %d j %d edge %d first %d last %d\n", self, j, edge, first, last);
      if(edge >= first && edge < last) {
          // If the adjacent edge has been visited and either
          // (1) source is unknown or
          // (2) source is known (implicit) and
          //     depth of old source edge is greater than current edge
          // then update the source.
          if (depth[edge] != -1 &&
                  (source == -1 || depth[edge] < depth[source]))
              source = edge;
      }
    }
    // If a source has been found, this work-item has control of it, and 
    //  the level is the current one:
    if (source!=-1 && depth[source]==level) {
      // loop over all edges adjacent to the vertex and call the visit
      // function
      for (lid_t j = degreeList[self]; j < degreeList[self+1]; j++){
        lid_t edge = edgeList[j];
        num_updates+=depth_visit(depth,source,edge);
      }
    }
    num_updates = reductionSum(num_updates, localWork);
    if( !self )
      printf("device: num_updates %d\n", num_updates);
    level++;
  } while(num_updates);
}
