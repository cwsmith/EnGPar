kernel void bfskernel(global int *verts)
{
  uint i       = get_global_id(0);
  uint lid     = get_local_id(0);
  if( !i )
    printf("hello from worker zero!\n");
  verts[i] = verts[i] + 1;
}
