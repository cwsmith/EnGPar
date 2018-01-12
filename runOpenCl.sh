#!/bin/bash
usage() {
  echo "Usage: $0 <device> <bfsmode> <debug=on|off> <oclgrind=on|off>"
  ./testDistanceQueue --help
  ./testDistanceQueue --list
}
device=$1
bfsmode=$2
debug=$3
grind=$4

cd test
cp /Users/cwsmith/develop/engpar/partition/Diffusive/src/bfskernel.cl .

[ $# -ne 4 ] && usage && exit 1

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

${!oclgrind} ${!gdb} ./testDistanceQueue ${device} --bfsmode ${bfsmode}

set +x

cd -
