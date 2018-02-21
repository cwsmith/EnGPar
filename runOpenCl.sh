#!/bin/bash
usage() {
  echo "Usage: $0 <kernel> <device> <bfsmode> </absolute/path/to/graph/file.bgd> <debug=on|off> <oclgrind=on|off>"
  ./testDistanceQueue --help
  ./testDistanceQueue --list
}
kernel=$1
device=$2
bfsmode=$3
graph=$4
debug=$5
grind=$6

cd test

[ $# -ne 6 ] && usage && exit 1

[ ! -e $kernel ] && echo "kernel path '$kernel' does not exist" && usage && exit 1

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

${!oclgrind} ${!gdb} ./testDistanceQueue --kernel $kernel ${device} ${grapharg} --bfsmode ${bfsmode}

set +x

cd -
