// vim :set ts=2 sw=2 sts=2 et :

typedef uint lid_t;

bool visited(lid_t edge, global int* depth, int* depthChecks);

// edges that have not been visited have a depth of -1
bool visited(lid_t edge, global int* depth, int* depthChecks) {
    (*depthChecks)++;
    return (depth[edge] != -1);
}

kernel void bfsScgKernel(global long* degreeList,
                     global long* edgeList,
                     global int* depth,
                     global int* frontSize,
                     const int numVerts,
                     const int level)
{
  int depthChecks = 0;
  lid_t numChunks = get_num_groups(0); // number of chunks
  lid_t chunkLength = get_local_size(0); // number of verts in a chunk
  lid_t chunk = get_group_id(0); // which chunk is this work-item in
  lid_t lid = get_local_id(0); // which vert in the chunk is assigned to this work-item

  lid_t globalSize = get_global_size(0); // number of verts; can be padded
  lid_t gid = get_global_id(0); // global vtx id, can be into padding - need to compare against num verts

  const lid_t LARGE_DEPTH = 1024*1024*1024;

  //no work here - just a padded global entry
  if( gid >= numVerts ) return;

  const lid_t chunkStart = degreeList[chunk];
  const lid_t maxChunkDeg = (degreeList[chunk+1] - degreeList[chunk])/chunkLength;
  const lid_t chunkSize = chunkLength*maxChunkDeg;
  const lid_t firstEdgeIdx = chunkStart+lid;
  const lid_t lastEdgeIdx = firstEdgeIdx+chunkSize;

  // loop through the edges adjacent to the vertex and find
  // the one with the smallest depth
  lid_t minDepthEdge = -1;
  int minDepth = LARGE_DEPTH;
  for (lid_t j = firstEdgeIdx; j < lastEdgeIdx; j += chunkLength) {
    const lid_t edge = edgeList[j];
    // skip padded entries/edges
    if (edge == -1) continue;
    if (visited(edge,depth,&depthChecks) ) {
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
    for (lid_t j = firstEdgeIdx; j < lastEdgeIdx; j += chunkLength) {
      const lid_t edge = edgeList[j];
      // skip padded entries/edges
      if (edge == -1) continue;
      if (!visited(edge,depth,&depthChecks)) {
        depth[edge] = minDepth+1;
        atomic_inc(frontSize);
      }
    }
  }
#ifdef KERNEL_DEBUG
  if ( ! get_global_id(0) )
      printf("depthChecks %d\n", depthChecks);
#endif
}
