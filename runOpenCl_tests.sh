#!/bin/bash
usage() {
  echo "Usage: $0 /path/to/graph/dir /path/to/testDistanceQueue /path/to/EnGPar/source/dir"
}
graphDir=$1
bin=$2
srcDir=$3

[ $# -ne 3 ] && usage && exit 1

[ ! -e $graphDir ] && "$graphDir does not exist" && exit 1
[ ! -e $bin ] && "$bin does not exist" && exit 1
[ ! -e $srcDir ] && "$srcDir does not exist" && exit 1

cp $srcDir/partition/Diffusive/src/*.cl .

#aero1Belm/graph128Ki_88637
#aero1Belm/graph128Ki_25114
#aero1Belm/afterMigr_81334
#aero1Belm/afterMigr_1457
#cube/cube
read -r -d '' tests <<'EOF'
upright/67k
upright/190k
upright/400k
upright/890k
upright/1.6M
upright/13M
upright/28M
EOF

chunkSizes=(128 256 512 1024 2048)
bfsModeString=("push" "pull" "csropencl" "scgopencl")
bfsModeIdx=(0 1 2 3)

for bfsmode in ${bfsModeIdx[@]}; do
  echo "${bfsModeString[$bfsmode]} bfs"
  kernel=""
  [ $bfsmode == 2 ] && kernel="--kernel bfsCsrKernel.cl"
  [ $bfsmode == 3 ] && kernel="--kernel bfsScgKernel.cl"
  chunkRange=(1)
  [ $bfsmode == 3 ] && chunkRange=${chunkSizes[@]}
  for t in ${tests[@]}; do
    graphName=${t##*/}
    for c in ${chunkRange[@]}; do
      echo graph $t bfsmode $bfsmode $kernel chunk $c
      for i in {1..3}; do
        $bin --device 0 --graph ${graphDir}/$t --bfsmode $bfsmode $kernel --chunkSize $c >> ${bfsModeString[$bfsmode]}_chunk${c}_${graphName}.log
      done
    done
  done
done

cmdString=('/push bfs time/ {sum+=$6; cnt+=1} END {print sum/cnt}' 
           '/pull bfs time/ {sum+=$6; cnt+=1} END {print sum/cnt}'
           '/^opencl bfs time/ {sum+=$5; cnt+=1} END {print sum/cnt}'
           '/^opencl bfs time/ {sum+=$5; cnt+=1} END {print sum/cnt}')
for bfsmode in ${bfsModeIdx[@]}; do
  outlog=${bfsModeString[$bfsmode]}AvgTimes.csv
  cat /dev/null > $outlog
  echo "${bfsModeString[$bfsmode]} bfs"
  chunkRange=(1)
  [ $bfsmode == 3 ] && chunkRange=${chunkSizes[@]}
  for t in ${tests[@]}; do
    graphName=${t##*/}
    for c in ${chunkRange[@]}; do
      inlog=${bfsModeString[$bfsmode]}_chunk${c}_${graphName}.log
      echo graph $t bfsmode $bfsmode chunk $c inlog $inlog outlog $outlog
      avg=$(awk "${cmdString[$bfsmode]}" $inlog)
      echo "$graphName,$c,$avg" >> $outlog
    done
  done
done

##print number of vertices and edges in each test graph
#outlog=graphs.csv
#cat /dev/null > $outlog
#for t in ${tests[@]}; do
#  graphName=${t##*/}
#  vertsAndEdges=$(awk '/numVtx/ {print $7","$8; exit}' opencl_${graphName}.log)
#  echo "$graphName,$vertsAndEdges" >> $outlog
#done
