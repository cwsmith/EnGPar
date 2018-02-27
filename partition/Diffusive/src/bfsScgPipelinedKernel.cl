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

#define updateDepth(edge, depth, nextDepth, changes) { \
  const int d = (depth[(edge)]);                        \
  if ( d == -1 ) {  /*not visited*/                     \
    (depth[(edge)]) = nextDepth;                        \
    (changes) = true;                            \
  }                                                     \
}                                                       \

kernel void bfsScgPipelinedKernel(global lid_t* restrict degreeList,
                     global lid_t* restrict edgeList,
                     global int* restrict depth,
                     const int numChunks,
                     const int chunkLength,
                     const int numVerts,
                     const int start_depth)
{
  const lid_t LARGE_DEPTH = 1024*1024*1024;
  int maxLevel=1000;
  int level=start_depth;
  int changes;
  do {
    int vtx = 0;
    changes = false;
    for(int chunk=0; chunk < numChunks; chunk++) {
      const lid_t chunkStart = degreeList[chunk];
      const lid_t maxChunkDeg = (degreeList[chunk+1] - degreeList[chunk])/chunkLength;
      const lid_t chunkSize = chunkLength*maxChunkDeg;

      for(int j=0; j<chunkLength; j++) {
        vtx++;
        if (vtx > numVerts) 
            continue;
        const lid_t firstEdgeIdx = chunkStart+j;
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
          updateDepth(edgeList[firstEdgeIdx], depth, nextDepth, changes);
          updateDepth(edgeList[firstEdgeIdx+chunkLength], depth, nextDepth, changes);
          updateDepth(edgeList[firstEdgeIdx+chunkLength*2], depth, nextDepth, changes);
          updateDepth(edgeList[firstEdgeIdx+chunkLength*3], depth, nextDepth, changes);
#else
          // a visited edge was found - loop through the adjacent 
          // edges again and set the depth of unvisited edges
          for (lid_t j = firstEdgeIdx; j < lastEdgeIdx; j += chunkLength) {
            const lid_t edge = edgeList[j];
            // skip padded entries/edges
            if (edge == -1) continue;
            updateDepth(edge,depth,nextDepth,changes);
          }
#endif
        }
      }
    }
    printf("level %d changes %d\n", level, changes);
    level++;
  } while(changes && level < maxLevel);
}
