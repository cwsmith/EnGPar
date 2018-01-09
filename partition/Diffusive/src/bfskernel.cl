typedef long lid_t;


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
int depth_visit(global long* depth, const long source, const long dest) {
    if (depth[dest]==-1) {
       depth[dest] = depth[source]+1;
       return 1;
    } else {
       return 0;
    }
}

kernel void bfskernel(global long* degreeList,
                      global long* edgeList,
                      const long numEdges,
                      global int* depth,
                      global int* seeds,
                      const long numSeeds,
                      const int startDepth,
                      local int* localWork)
{
  uint i       = get_global_id(0);
  uint lid     = get_local_id(0);
  uint dim     = get_work_dim();
  uint globalSize[3] = {0, 0, 0};
  uint localSize[3] = {0, 0, 0};
  for(uint d=0; d<dim; d++) {
     localSize[d] = get_local_size(d);
     globalSize[d] = get_global_size(d);
  }
  if( !i ) {
    printf("device: numEdges numSeeds startDepth %ld %ld %d\n",
           numEdges, numSeeds, startDepth);
    printf("device: localsize globalsize ");
    for(uint d=0; d<dim; d++)
      printf(" <%d> %d %d ", d, localSize[d], globalSize[d]);
    printf("\n");
  }

  int num_updates = 0;
  int level = 0;
  do {
    if( !i )
      printf("device: level %d\n", level);
    num_updates = 0;
    int source = -1;
    for (lid_t j = degreeList[i]; j < degreeList[i+1]; j++){
      lid_t edge = edgeList[j];
      // If the adjacent edge has been visited and either
      // (1) source is unknown or
      // (2) source is known (implicit) and
      //     depth of old source edge is greater than current edge
      // then update the source.
      if (depth[edge] != -1 &&
          (source == -1 || depth[edge] < depth[source]))
        source = edge;
    }
    // If a source has been found, and the level is the current one
    if (source!=-1 && depth[source]==level) {
      // loop over all edges adjacent to the vertex and call the visit
      // function
      for (lid_t j = degreeList[i]; j < degreeList[i+1]; j++){
        lid_t edge = edgeList[j];
        num_updates+=depth_visit(depth,source,edge);
      }
    }
    num_updates = reductionSum(num_updates, localWork);
    level++;
  } while(num_updates);
}
