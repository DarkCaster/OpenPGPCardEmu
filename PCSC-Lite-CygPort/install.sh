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

cd "$curdir/pcsc-lite"
find . -type f -name "*.tar.xz" -exec tar -C/ -Jxf {} \;
