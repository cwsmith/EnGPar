typedef long lid_t;

void printInputs(global long* degreeList,
        global long* edgeList,
        const long numEdges,
        const long numPins,
        global int* depth);
void printDimensions(void);
bool visited(lid_t edge, global int* depth);

void printInputs(global long* degreeList,
        global long* edgeList,
        const long numEdges,
        const long numPins,
        global int* depth) {
  uint self = get_global_id(0);
  if( !self ) {
    printf("\n\n----------------------------------------------\n");
    printf("device: numEdges numPins %ld %ld\n",
            numEdges, numPins);
    printf("device: degreeList ");
    for(size_t i = 0; i < get_global_size(0)+1; i++)
        printf("%ld ", degreeList[i]);
    printf("\n");
    printf("device: edgeList ");
    for(long i = 0; i < numPins; i++)
        printf("%ld ", edgeList[i]);
    printf("\n");
    printf("device: depth ");
    for(int i = 0; i < numEdges; i++)
        printf("%d ", depth[i]);
    printf("\n");
    printf("----------------------------------------------\n\n");
  }
  barrier(CLK_LOCAL_MEM_FENCE|CLK_GLOBAL_MEM_FENCE);
}

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

// edges that have not been visited have a depth of -1
bool visited(lid_t edge, global int* depth) {
    return (depth[edge] != -1);
}

kernel void bfskernel(global long* degreeList,
                     global long* edgeList,
                     global int* depth,
                     global char* changes,
                     const int level)
{
  uint gid = get_global_id(0);
  const int LARGE_DEPTH = 1024*1024*1024;
  const int firstEdge=degreeList[gid];
  const int lastEdge=degreeList[gid+1];

  // loop through the edges adjacent to the vertex and find
  // the one with the smallest depth
  lid_t minDepthEdge = -1;
  int minDepth = LARGE_DEPTH;
  for (lid_t j = firstEdge; j < lastEdge; j++){
    lid_t edge = edgeList[j];
    if (visited(edge,depth) ) {
      if (depth[edge] < minDepth) {
        // this edge has the lowest depth of those adjacent to vtx 'gid'
        minDepthEdge = edge;
        minDepth = depth[edge];
      }
    }
  }
  if (minDepth == level) {
    // a visited edge was found - loop through the adjacent 
    // edges again and set the depth of unvisited edges
    for (lid_t j = firstEdge; j < lastEdge; j++){
      lid_t edge = edgeList[j];
      if (!visited(edge,depth)) {
          depth[edge] = minDepth+1;
          *changes = true;
      }
    }
  }
}
