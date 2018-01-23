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

cp $srcDir/partition/Diffusive/src/bfskernel.cl .

read -r -d '' tests <<'EOF'
aero1Belm/graph128Ki_88637
aero1Belm/graph128Ki_25114
aero1Belm/afterMigr_81334
aero1Belm/afterMigr_1457
cube/cube
upright/67k
upright/190k
upright/400k
upright/890k
upright/1.6M
EOF

bfsModeString=("push" "pull" "opencl")
for bfsmode in 0 1 2; do
  echo "${bfsModeString[$bfsmode]} bfs"
  for t in ${tests[@]}; do
    graphName=${t##*/}
    for i in {1..5}; do
      $bin --device 0 --graph ${graphDir}/$t --bfsmode $bfsmode >> ${bfsModeString[$bfsmode]}_${graphName}.log
    done
  done
done

cmdString=('/push bfs time/ {sum+=$6; cnt+=1} END {print sum/cnt}' 
           '/pull bfs time/ {sum+=$6; cnt+=1} END {print sum/cnt}'
           '/^opencl bfs time/ {sum+=$5; cnt+=1} END {print sum/cnt}')
for bfsmode in 0 1 2; do
  outlog=${bfsModeString[$bfsmode]}AvgTimes.csv
  cat /dev/null > $outlog
  echo "${bfsModeString[$bfsmode]} bfs"
  for t in ${tests[@]}; do
    graphName=${t##*/}
    inlog=${bfsModeString[$bfsmode]}_${graphName}.log
    avg=$(awk "${cmdString[$bfsmode]}" $inlog)
    echo "$graphName,$avg" >> $outlog
  done
done

#print number of vertices and edges in each test graph
outlog=graphs.csv
cat /dev/null > $outlog
for t in ${tests[@]}; do
  graphName=${t##*/}
  vertsAndEdges=$(awk '/numVtx/ {print $7","$8; exit}' opencl_${graphName}.log)
  echo "$graphName,$vertsAndEdges" >> $outlog
done
