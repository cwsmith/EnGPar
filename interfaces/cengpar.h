#ifndef CENGPAR
#define CENGPAR

#include <mpi.h>
#include "agi.h"

#ifdef __cplusplus
extern "C" {
#endif

void cengpar_initialize();
void cengpar_finalize();
void cengpar_setftncommunicator(MPI_Fint fcomm);

typedef void* ngraph;

ngraph cengpar_createEmptyGraph();

void cengpar_destroyGraph(ngraph g);

void cengpar_constructVerts(ngraph g, bool isHg,
    agi::gid_t* verts, agi::wgt_t* weights, int nverts);

void cengpar_constructEdges(ngraph g, agi::gid_t* edges,
    agi::lid_t* degs, agi::wgt_t* weights, agi::gid_t* pins,
    int nedges, int npins);

void cengpar_constructGhosts(ngraph g, agi::gid_t* verts,
    agi::part_t* owners, int nghosts);

void cengpar_checkValidity(ngraph g);

void cengpar_balanceVertices(ngraph g, double tol,
    double stepfactor, int verbosity);

void cengpar_getPartition(ngraph g, agi::gid_t* verts,
    agi::part_t* parts, int nverts);
#ifdef __cplusplus
}
#endif

#endif
