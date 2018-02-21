typedef long lid_t;

bool visited(lid_t edge, global int* depth);

// edges that have not been visited have a depth of -1
bool visited(lid_t edge, global int* depth) {
    return (depth[edge] != -1);
}

kernel void bfsCsrKernel(global long* degreeList,
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
