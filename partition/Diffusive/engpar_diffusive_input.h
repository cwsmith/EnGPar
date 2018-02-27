#ifndef __ENGPAR_DIFFUSIVE_INPUT_H__
#define __ENGPAR_DIFFUSIVE_INPUT_H__

#include <string>
#include <ngraph.h>
#include "../engpar_input.h"
namespace engpar {
  
  class DiffusiveInput : public Input {
  public:
    DiffusiveInput(agi::Ngraph*);

    void addPriority(int type, double tolerance);

    /** \brief The order of graph entities to be balanced 
     *
     *  -1 represents graph vertices, 0-MAX_TYPES represent edge_types
     */
    std::vector<int> priorities;

    /** \brief The imbalance tolerance for each priority
     *
     * tolerances must be given in the same order as in the priorities vector
     */
    std::vector<double> tolerances;

    /** \brief The maximum iterations for all load balancing */
    int maxIterations;

    /** \brief The maximum iterations of load balancing foreach graph entity type 
     *
     *  defaults to 100
     */
    int maxIterationsPerType;

    /** \brief The percent of difference in weight to send in each iteration 
     *
     * defaults to .1 
     */
    double step_factor;
    
    /** \brief The edge type used for determining part boundaries 
     *
     * defaults to 0
     */
    int sides_edge_type;

    /** \brief The edge type used for creating cavities for selection 
     *
     * defaults to 0
     */
    int selection_edge_type;

    /** \brief If ghosts should be accounted for in the weight of a part 
     *
     * defaults to false
     */
    bool countGhosts;

    /** \brief If the distance based iteration queue should be used.
     *
     * defaults to false (for now)
     */
    bool useDistanceQueue;

    /** \brief use the push bfs
     *
     * defaults to false
     */
    bool bfsPush;

    /** \brief use the pull bfs
     *
     * defaults to true
     */
    bool bfsPull;

    /** \brief use the pull bfs on the CSR written in OpenCL
     *
     * defaults to false
     */
    bool bfsCsrOpenCL;

    /** \brief use the pull bfs on the Sell-C-Sigma written in OpenCL
     *
     * defaults to false
     */
    bool bfsScgOpenCL;

    /** \brief use the pipelined opencl bfs
     *
     * defaults to false
     */
    bool isPipelined;

    /** \brief path to the OpenCL kernel
     *
     * defaults to an empty string
     */
    std::string kernel;

    /** \brief size of SCG chunk
     *
     * defaults to 1
     */
    int chunkSize;
  };
}

#endif
