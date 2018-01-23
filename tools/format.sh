#!/bin/bash
set -e

dirt=`pwd | grep 'tools' | wc -l`
if [ "$dirt" -gt 0 ];then
    cd ..
fi

find $1 -type f | xargs dos2unix
find $1 -type f | xargs astyle -A3s4SNwm2M40fpHUjk3n
