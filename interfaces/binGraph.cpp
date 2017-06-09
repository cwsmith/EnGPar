#include "binGraph.h"
#include <mpi.h>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sstream>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <PCU.h>
#include <iostream>
namespace agi {

Ngraph* createBinGraph(char* graph_file,char* part_file) {
  return new binGraph(graph_file,part_file);
}

  //TODO: replace mallocs with new
  //TODO: replace mpi calls with PCU
binGraph::binGraph(char* graph_file,char* part_file) : Ngraph() {
  uint64_t* read_edges;
  uint64_t m_read;
  etype t = load_edges(graph_file,read_edges,m_read);
  int32_t* ranks = (int32_t*)malloc(num_global_verts*sizeof(int32_t));
  if (!part_file)
    vert_block_ranks(ranks);
  else
    read_ranks(part_file,ranks);
  exchange_edges(m_read,read_edges,ranks,t);
  create_dist_csr(ranks,t);
  std::vector<wgt_t> wgts;
  setEdgeWeights(wgts,0);
}

  void phi() {
    if (!PCU_Comm_Self()) {
      printf("hi\n");
    }
  }
void binGraph::destroyData() {
  //cleanup any additional memory
}

void binGraph::migrate(agi::EdgePartitionMap& map) {
  EdgePartitionMap::iterator itr;
  PCU_Comm_Begin();
  for (itr = map.begin();itr!=map.end();itr++) {
    lid_t lid = itr->first;
    gid_t v1 = local_unmap[u(lid)];
    gid_t v2 = local_unmap[edge_list[0][lid]];
    PCU_COMM_PACK(itr->second,v1);
    PCU_COMM_PACK(itr->second,v2);
  }
  PCU_Comm_Send();
  std::vector<gid_t> recv_edges;
  while (PCU_Comm_Receive()) {
    gid_t v1;
    PCU_COMM_UNPACK(v1);
    recv_edges.push_back(v1);
  }
  num_global_verts = PCU_Add_Long(num_global_verts);
  if (numEdgeTypes()==0)
    addEdgeType();

  num_local_edges[0] = recv_edges.size()/2;
  num_global_edges[0] = PCU_Add_Long(num_local_edges[0]);
  
  if (edge_list[0])
    delete edge_list[0];
  edge_list[0] = new gid_t[num_local_edges[0]*2];
  std::copy(recv_edges.begin(),recv_edges.end(),edge_list[0]);
  vtx_mapping.clear();
  int32_t* ranks = (int32_t*)malloc(num_global_verts*sizeof(int32_t));
  for (gid_t i=0;i<num_global_verts;i++)
    ranks[i] = -1;
  for (lid_t i=0;i<num_local_edges[0]*2;i++) {
    ranks[edge_list[0][i]] = PCU_Comm_Self();
  }

  create_dist_csr(ranks,0,false);
  delete [] ranks;

  //TODO: Make much more efficient
  PCU_Comm_Begin();
  for (lid_t i=0;i<num_local_verts;i++) {
    for (int j=1;j<PCU_Comm_Peers();j++)
      PCU_COMM_PACK((PCU_Comm_Self()+j)%PCU_Comm_Peers(),local_unmap[i]);
  }
  PCU_Comm_Send();
  std::vector<part_t> owns;
  std::vector<gid_t> dups;
  degree_list[SPLIT_TYPE] = new lid_t[num_local_verts+1];
  for (lid_t i=0;i<num_local_verts+1;++i)
    degree_list[SPLIT_TYPE][i]=0;

  while (PCU_Comm_Receive()) {
    gid_t gid;
    PCU_COMM_UNPACK(gid);
    map_t::iterator itr = vtx_mapping.find(gid);
    if (itr!=vtx_mapping.end()) {
      dups.push_back(gid);
      owns.push_back(PCU_Comm_Sender());
      degree_list[SPLIT_TYPE][itr->second+1]++;
    }
  }

  for (lid_t i=1;i<num_local_verts+1;++i)
    degree_list[SPLIT_TYPE][i]+=degree_list[SPLIT_TYPE][i-1];

  assert(degree_list[SPLIT_TYPE][num_local_verts] ==dups.size());
  num_ghost_verts = dups.size();
  num_local_edges[SPLIT_TYPE] = dups.size();

  ghost_unmap = new gid_t[dups.size()];
  owners = new part_t[dups.size()];
  uint64_t* temp_counts = (uint64_t*)malloc(num_local_verts*sizeof(uint64_t));
  memcpy(temp_counts, degree_list[SPLIT_TYPE], num_local_verts*sizeof(uint64_t));

  edge_list[SPLIT_TYPE] = new lid_t[dups.size()];
  
  for (unsigned int i=0;i<dups.size();i++) {
    lid_t lid = vtx_mapping[dups[i]];
    edge_list[SPLIT_TYPE][temp_counts[lid]++] = num_local_verts+i;
    ghost_unmap[i]=dups[i];
    owners[i] = owns[i];
  }
  num_global_edges[SPLIT_TYPE] = PCU_Add_Long(num_local_edges[SPLIT_TYPE]);
}

  //TODO: optimize these operations using PCU
//Private Functions
etype binGraph::load_edges(char *filename, uint64_t*& read_edges,
    uint64_t& m_read) {  
  FILE *infp = fopen(filename, "rb");
  fseek(infp, 0L, SEEK_END);
  uint64_t file_size = ftell(infp);
  fseek(infp, 0L, SEEK_SET);
  etype t = addEdgeType();
  num_global_edges[t] = file_size/(2*sizeof(uint32_t));
  
  uint64_t read_offset_start = PCU_Comm_Self()*2*sizeof(uint32_t)*
    (num_global_edges[t] / (uint64_t)PCU_Comm_Peers());
  uint64_t read_offset_end = (PCU_Comm_Self()+1)*2*sizeof(uint32_t)*
    (num_global_edges[t] / (uint64_t)PCU_Comm_Peers());

  if (PCU_Comm_Self() == PCU_Comm_Peers() - 1)
    read_offset_end = 2*sizeof(uint32_t)*num_global_edges[t];

  m_read = (read_offset_end - read_offset_start)/(2*sizeof(uint32_t));

  uint32_t* temp_read = (uint32_t*)malloc(2*m_read*sizeof(uint32_t));
  read_edges = (uint64_t*)malloc(2*m_read*sizeof(uint64_t));
  fseek(infp, read_offset_start, SEEK_SET);
  fread(temp_read, m_read, 2*sizeof(uint32_t), infp);
  fclose(infp);
  for (uint64_t i = 0; i < m_read*2; ++i)
    read_edges[i] = (uint64_t)temp_read[i];
  free(temp_read);

  num_global_verts = 0;
  for (uint64_t i = 0; i < m_read*2; ++i)
    if (read_edges[i] > num_global_verts) {
      num_global_verts = read_edges[i];
    }

  MPI_Allreduce(MPI_IN_PLACE, &num_global_verts, 1, MPI_UINT64_T, MPI_MAX, PCU_Get_Comm());
 
  num_global_verts += 1;

  return t;
}

int binGraph::read_ranks(char* filename, int32_t* ranks) {
  std::ifstream infile;
  std::string line;
  infile.open(filename);

  for (uint64_t i = 0; i < num_global_verts; ++i)
  {
    getline(infile, line);
    ranks[i] = (int32_t)atoi(line.c_str());
  }
  infile.close();

  return 0;
}

int binGraph::vert_block_ranks(int32_t* ranks)
{
  uint64_t n_per_rank = num_global_verts / (uint64_t)PCU_Comm_Peers() + 1;
  for (uint64_t i = 0; i < num_global_verts; ++i)
    ranks[i] = i / n_per_rank;

  return 0;
}


int binGraph::exchange_edges(uint64_t m_read, uint64_t* read_edges,
                             int32_t* ranks,etype t)
{
  int32_t* scounts = (int32_t*)malloc(PCU_Comm_Peers()*sizeof(int32_t));
  int32_t* rcounts = (int32_t*)malloc(PCU_Comm_Peers()*sizeof(int32_t));
  int32_t* sdispls = (int32_t*)malloc(PCU_Comm_Peers()*sizeof(int32_t));
  int32_t* sdispls_cpy = (int32_t*)malloc(PCU_Comm_Peers()*sizeof(int32_t));
  int32_t* rdispls = (int32_t*)malloc(PCU_Comm_Peers()*sizeof(int32_t));
  for (int i = 0; i < PCU_Comm_Peers(); ++i)
  {
    scounts[i] = 0;
    rcounts[i] = 0;
    sdispls[i] = 0;
    sdispls_cpy[i] = 0;
    rdispls[i] = 0;
  }

  for (uint64_t i = 0; i < m_read*2; i+=2)
  {
    uint64_t vert = read_edges[i];
    int vert_task = ranks[vert];
    scounts[vert_task] += 2;
  }

  MPI_Alltoall(scounts, 1, MPI_INT32_T,
               rcounts, 1, MPI_INT32_T, PCU_Get_Comm());

  for (int i = 1; i < PCU_Comm_Peers(); ++i) {
    sdispls[i] = sdispls[i-1] + scounts[i-1];
    sdispls_cpy[i] = sdispls[i];
    rdispls[i] = rdispls[i-1] + rcounts[i-1];
  }
 
  int32_t total_send = sdispls[PCU_Comm_Peers()-1] + scounts[PCU_Comm_Peers()-1];
  int32_t total_recv = rdispls[PCU_Comm_Peers()-1] + rcounts[PCU_Comm_Peers()-1];
  uint64_t* sendbuf = (uint64_t*)malloc(total_send*sizeof(uint64_t));
  edge_list[t] = (uint64_t*)malloc(total_recv*sizeof(uint64_t));
  num_local_edges[t] = total_recv / 2;

  for (uint64_t i = 0; i < m_read*2; i+=2)
  {
    uint64_t vert1 = read_edges[i];
    uint64_t vert2 = read_edges[i+1];
    int vert_task = ranks[vert1];

    sendbuf[sdispls_cpy[vert_task]++] = vert1;
    sendbuf[sdispls_cpy[vert_task]++] = vert2;
  }

  MPI_Alltoallv(sendbuf, scounts, sdispls, MPI_UINT64_T,
                edge_list[t], rcounts, rdispls, MPI_UINT64_T, PCU_Get_Comm());
  free(sendbuf);

  return 0;
}


  int binGraph::create_dist_csr(int32_t* ranks,etype t,bool createGhost)
{
  num_local_verts = 0;

  for (uint64_t i = 0; i < num_global_verts; ++i)
    if (ranks[i] == PCU_Comm_Self())
      ++num_local_verts;

  local_unmap = (uint64_t*)malloc(num_local_verts*sizeof(uint64_t));
  uint64_t cur_label = 0;
  for (uint64_t i = 0; i < num_local_edges[t]*2; i++) {
    uint64_t out = edge_list[t][i];


    
    if (ranks[out] != PCU_Comm_Self())
      continue;
    if (vtx_mapping.count(out) == 0) {
      vtx_mapping[out] = cur_label;
      local_unmap[cur_label] = out;
      edge_list[t][i] = cur_label++;
    }
    else        
      edge_list[t][i] = vtx_mapping[out];
  }
  uint64_t* tmp_edges = (uint64_t*)malloc(num_local_edges[t]*sizeof(uint64_t));
  uint64_t* temp_counts = (uint64_t*)malloc(num_local_verts*sizeof(uint64_t));
  degree_list[t] = (uint64_t*)malloc((num_local_verts+1)*sizeof(uint64_t));
  for (uint64_t i = 0; i < num_local_verts+1; ++i)
    degree_list[t][i] = 0;
  for (uint64_t i = 0; i < num_local_verts; ++i)
    temp_counts[i] = 0;
  for (uint64_t i = 0; i < num_local_edges[t]*2; i+=2)
    ++temp_counts[edge_list[t][i]];
  for (uint64_t i = 0; i < num_local_verts; ++i)
    degree_list[t][i+1] = degree_list[t][i] + temp_counts[i];
  memcpy(temp_counts, degree_list[t], num_local_verts*sizeof(uint64_t));
  for (uint64_t i = 0; i < num_local_edges[t]*2; i+=2)
    tmp_edges[temp_counts[edge_list[t][i]]++] = edge_list[t][i+1];
  free(edge_list[t]);
  edge_list[t] = tmp_edges;
  if (createGhost) {
    cur_label = num_local_verts;
    std::vector<uint64_t> nonlocal_vids;
    for (uint64_t i = 0; i < num_local_edges[t]; ++i) {
      uint64_t out = edge_list[t][i];
      if (vtx_mapping.find(out) ==vtx_mapping.end()) {
	vtx_mapping[out] = cur_label;
	edge_list[t][i] = cur_label++;
	nonlocal_vids.push_back(out);
      }
      else        
	edge_list[t][i] = vtx_mapping[out];
    }
    num_ghost_verts = cur_label - num_local_verts;
    if (num_ghost_verts>0) {
      ghost_unmap = (uint64_t*)malloc(num_ghost_verts*sizeof(uint64_t));
      owners = (int32_t*)malloc(num_ghost_verts*sizeof(int32_t));
      for (uint64_t i = 0; i < nonlocal_vids.size(); ++i) {
	ghost_unmap[i] = nonlocal_vids[i];
	owners[i] = ranks[nonlocal_vids[i]];
      }
    }
  }

  return 0;
}


}

