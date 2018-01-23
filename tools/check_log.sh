#!/bin/bash

param=$1

dirt=`pwd | grep 'tools' | wc -l`
if [ "$dirt" -gt 0 ];then
    cd ..
fi

if [ "$param" = "crash" ] ; then
    grep -n "Backtrace:" ./serial_log/*.log
elif [ "$param" = "reboot" ] ; then
    grep -n "boot:" serial_log/*.log | grep "2nd stage bootloader"
elif [ "$param" = "heap" ] ; then
    grep -n "Heap summary for capabilities" ./serial_log/*.log
elif [ "$param" = "clr" ] ; then
    for file in ./serial_log/*.log
    do
        echo "clear log content ..." > $file
    done
else
    echo -e "\033[32m-------------------help info-----------------"
    echo " paramters:"
    echo "   'crash': detect whether devices crash"
    echo "  'reboot': detect whether devices reboot"
    echo "    'heap': detect whether devices malloc error"
    echo "     'clr': clear log files in directory ./serial_log/*"
    echo -e "---------------------------------------------\033[00m"
fi
