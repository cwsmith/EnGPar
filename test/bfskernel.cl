kernel void bfskernel()
{
  uint i       = get_global_id(0);
  uint lid     = get_local_id(0);
  if( !i )
    fprintf(stderr, "hello from worker zero!\n");
}
