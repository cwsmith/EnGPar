kernel void bfskernel(global long* degreeList,
                      global long* edgeList,
                      global long* pinDegreeList,
                      global long* pinList,
                      const long numVerts,
                      const long numEdges,
                      const long numPins)
{
  uint i       = get_global_id(0);
  uint lid     = get_local_id(0);
  if( !i ) {
    printf("device: numVerts numEdges numPins %ld %ld %ld\n",
           numVerts, numEdges, numPins);
  }
}
