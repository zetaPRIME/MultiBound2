#!/usr/bin/env bash

# Script for Linux->Windows cross-compilation via MXE https://mxe.cc/
# Requires MXE packages: qtdeclarative and deps thereof
# This script assumes MXE's root directory is ../mxe

cd "${0%/*}" # make sure we're in the right directory

{
    mxeroot=$(realpath ../mxe)
    if [ ! -d "$mxeroot" ] ; then
        echo "MXE installation not found at '$mxeroot'"
        exit # don't try to compile without MXE
    fi
    
    mkdir -p ./xbuild
    cd ./xbuild
    
    export PATH=$mxeroot/usr/bin:$PATH
    $mxeroot/usr/i686-w64-mingw32.static/qt5/bin/qmake ../multibound2/multibound2.pro
    make clean
    make
}
