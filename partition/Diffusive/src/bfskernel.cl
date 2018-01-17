typedef long lid_t;

int depth_visit(global int* depth, const long source, const long dest);
void printInputs(global long* degreeList,
        global long* edgeList,
        const long numEdges,
        const long numPins,
        global int* depth);
void printDimensions(void);

//Visit function for first traversal
int depth_visit(global int* depth, const long source, const long dest) {
  if (depth[dest]==-1) {
     depth[dest] = depth[source]+1;
     return 1;
  } else {
     return 0;
  }
}

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

/* loop over the edges of the vertex associated with the work-item
 * if the work-item owns the vertex then record the depth of the vertex
 */
kernel void edgePull(global long* degreeList,
                     global long* edgeList,
                     const long numEdges,
                     const long numPins,
                     global int* depth,
                     global char* mask,
                     global char* maskupdates)
{
  uint gid = get_global_id(0);

  printDimensions();

  //printInputs(degreeList, edgeList, numEdges, numPins,
  //        depth);

  for (lid_t j = degreeList[gid]; j < degreeList[gid+1]; j++){
    lid_t edge = edgeList[j];
    if(edge >= first && edge < last) {
        // If the adjacent edge has been visited and either
        // (1) source is unknown or
        // (2) source is known (implicit) and
        //     depth of old source edge is greater than current edge
        // then update the source.
        if (depth[edge] != -1 &&
                (source == -1 || depth[edge] < depth[source]))
            mask[edge] = 
    }
  }
}

kernel void updateMask(global long* degreeList,
                      global long* edgeList,
                      const long numEdges,
                      const long numPins,
                      global int* depth,
                      global char* mask,
                      global char* maskupdates)
{
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
}
