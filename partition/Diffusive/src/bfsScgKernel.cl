// vim :set ts=2 sw=2 sts=2 et :

typedef int lid_t;

#define checkEdge(edge, depth, minDepthEdge, minDepth) { \
  const int d = (depth[(edge)]);                         \
  if ( d != -1 ) { /*visited*/                           \
    if ( d < minDepth) {                                 \
      /* this edge has the lowest */                     \
      /* depth of those adjacent to vtx 'gid' */         \
      (minDepthEdge) = (edge);                           \
      (minDepth) = d;                                    \
    }                                                    \
  }                                                      \
}                                                        \

#define updateDepth(edge, depth, nextDepth, frontSize) { \
  const int d = (depth[(edge)]);                        \
  if ( d == -1 ) {  /*not visited*/                     \
    (depth[(edge)]) = nextDepth;                        \
    atomic_inc((frontSize));                            \
  }                                                     \
}                                                       \

__attribute__((num_simd_work_items(16)))
kernel void bfsScgKernel(global lid_t* restrict degreeList,
                     global lid_t* restrict edgeList,
                     global int* restrict depth,
                     global int* restrict frontSize,
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
  lid_t minDepthEdge;
  int minDepth = LARGE_DEPTH;
#ifdef SCG_UNROLL
  checkEdge(edgeList[firstEdgeIdx], depth, minDepthEdge, minDepth);
  checkEdge(edgeList[firstEdgeIdx+chunkLength], depth, minDepthEdge, minDepth);
  checkEdge(edgeList[firstEdgeIdx+chunkLength*2], depth, minDepthEdge, minDepth);
  checkEdge(edgeList[firstEdgeIdx+chunkLength*3], depth, minDepthEdge, minDepth);
#else
  for (lid_t j = firstEdgeIdx; j < lastEdgeIdx; j += chunkLength) {
    const lid_t edge = edgeList[j];
    // skip padded entries/edges
    if (edge == -1) continue;
    checkEdge(edge, depth, minDepthEdge, minDepth);
  }
#endif

  if (minDepth == level) {
    const int nextDepth = level+1;
#ifdef SCG_UNROLL
    updateDepth(edgeList[firstEdgeIdx], depth, nextDepth, frontSize);
    updateDepth(edgeList[firstEdgeIdx+chunkLength], depth, nextDepth, frontSize);
    updateDepth(edgeList[firstEdgeIdx+chunkLength*2], depth, nextDepth, frontSize);
    updateDepth(edgeList[firstEdgeIdx+chunkLength*3], depth, nextDepth, frontSize);
#else
    // a visited edge was found - loop through the adjacent 
    // edges again and set the depth of unvisited edges
    for (lid_t j = firstEdgeIdx; j < lastEdgeIdx; j += chunkLength) {
      const lid_t edge = edgeList[j];
      // skip padded entries/edges
      if (edge == -1) continue;
      updateDepth(edge,depth,nextDepth,frontSize);
    }
#endif
  }
#ifdef KERNEL_DEBUG
  if ( ! get_global_id(0) )
      printf("depthChecks %d\n", depthChecks);
#endif
}
