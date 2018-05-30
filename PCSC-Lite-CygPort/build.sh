#!/bin/bash

[[ -z `which cygstart` ]] && echo "this script intended to be run with cygwin, cannot proceed!" && exit 1

curdir="$( cd "$( dirname "$0" )" && pwd )"

error() {
 local source="$1"
 local line="$2"
 local ec="$3"
 echo "*** command at $source:$line failed with error code $ec"
 exit $ec
}

trap 'error ${BASH_SOURCE} ${LINENO} $?' ERR

echo "building pcsc-lite with cygport"
cd "$curdir"
rm -rf pcsc-lite
cygport pcsc-lite-1.8.23-custom.cygport finish
cygport pcsc-lite-1.8.23-custom.cygport all
mv pcsc-lite-1.8.23-custom.*/dist/pcsc-lite pcsc-lite
cygport pcsc-lite-1.8.23-custom.cygport finish
