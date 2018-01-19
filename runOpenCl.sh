#!/bin/bash
usage() {
  echo "Usage: $0 <device> <bfsmode> </absolute/path/to/graph/file.bgd> <debug=on|off> <oclgrind=on|off>"
  ./testDistanceQueue --help
  ./testDistanceQueue --list
}
device=$1
bfsmode=$2
graph=$3
debug=$4
grind=$5

cd test
cp ~/develop/EnGPar/partition/Diffusive/src/bfskernel.cl .

[ $# -ne 5 ] && usage && exit 1

[[ "off" != "$debug" &&
   "on" != "$debug" ]] &&
   usage &&
   exit 1

[[ "off" != "$grind" &&
   "on" != "$grind" ]] &&
   usage &&
   exit 1


gdb_on="gdb --args"
gdb_off=""
gdb=gdb_${debug}

export PATH=$PATH:/Users/cwsmith/software/Oclgrind/build/install/bin
oclgrind_on="oclgrind"
oclgrind_off=""
oclgrind=oclgrind_${grind}

set -x

device="--device ${device}"
[ "on" == "$grind" ] && device=""

grapharg=""
if [[ "none" != "$graph" && -e "${graph}_0.bgd" ]]; then
  grapharg="--graph ${graph}"
fi

${!oclgrind} ${!gdb} ./testDistanceQueue ${device} ${grapharg} --bfsmode ${bfsmode}

set +x

cd -
