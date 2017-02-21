#!/bin/bash
#set -e
export IDF_PATH=$(pwd)/esp-idf
echo "----------------------"
echo "PLease Check IDF_PATH:" $IDF_PATH
echo "---------------------"

#sudo chmod 777 /dev/ttyUSB*
# make clean
make erase_flash flash -j8
# xtensa-esp32-elf-objdump -S ./build/alink.elf > bin/alink.s
minicom -c on
