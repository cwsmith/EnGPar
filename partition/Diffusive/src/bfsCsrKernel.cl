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

#define updateDepth(edge, depth, nextDepth, changes) { \
  const int d = (depth[(edge)]);                        \
  if ( d == -1 ) {  /*not visited*/                     \
    (depth[(edge)]) = nextDepth;                        \
    *(changes) = true;                                \
  }                                                     \
}

kernel void bfsCsrKernel(global lid_t* degreeList,
                     global lid_t* edgeList,
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
#ifdef CSR_UNROLL
  checkEdge(edgeList[firstEdge], depth, minDepthEdge, minDepth);
  checkEdge(edgeList[firstEdge+1], depth, minDepthEdge, minDepth);
  checkEdge(edgeList[firstEdge+2], depth, minDepthEdge, minDepth);
  checkEdge(edgeList[firstEdge+3], depth, minDepthEdge, minDepth);
#else
  for (lid_t j = firstEdge; j < lastEdge; j++){
    lid_t edge = edgeList[j];
    checkEdge(edgeList[j], depth, minDepthEdge, minDepth);
  }
#endif
  if (minDepth == level) {
    const int nextDepth = level+1;
    // a visited edge was found - loop through the adjacent 
    // edges again and set the depth of unvisited edges
#ifdef CSR_UNROLL
    updateDepth(edgeList[firstEdge], depth, nextDepth, changes);
    updateDepth(edgeList[firstEdge+1], depth, nextDepth, changes);
    updateDepth(edgeList[firstEdge+2], depth, nextDepth, changes);
    updateDepth(edgeList[firstEdge+3], depth, nextDepth, changes);
#else
    for (lid_t j = firstEdge; j < lastEdge; j++){
      updateDepth(edgeList[j], depth, nextDepth, changes);
    }
#endif
  }
}
