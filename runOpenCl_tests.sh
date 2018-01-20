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
EOF

bfsModeString=("push" "pull" "opencl")
for bfsmode in 0 1 2; do
  echo "${bfsModeString[$bfsmode]} bfs"
  for i in ${tests[@]}; do
    $bin --device 0 --graph ${graphDir}/$i --bfsmode $bfsmode >> ${bfsModeString[$bfsmode]}.log
  done
done
awk '/numEdges/ {print $6}' opencl.log
grep -a 'push bfs time' push.log
grep -a 'pull bfs time' pull.log
grep -a '^opencl bfs time' opencl.log
