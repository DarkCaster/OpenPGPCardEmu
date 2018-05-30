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

# build and install PCSC-Lite
"$curdir/PCSC-Lite-CygPort/build.sh"
"$curdir/PCSC-Lite-CygPort/install.sh"

echo "building openct driver"
cd "$curdir/OpenCTDriver"
./bootstrap
./configure --prefix=/usr --enable-pcsc --disable-usb
make -j1

echo "installing openct driver"
make install
