kernel void bfskernel(global int* verts, global int* degreeList)
{
  uint i       = get_global_id(0);
  uint lid     = get_local_id(0);
  if( !i )
    printf("hello from worker zero!\n");
  verts[i] = verts[i] + 1;
}
