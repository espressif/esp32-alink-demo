#!/bin/bash
set -e
export IDF_PATH=$(pwd)/esp-idf
echo "----------------------"
echo "PLease Check IDF_PATH"
echo "IDF_PATH"
echo $IDF_PATH
echo "---------------------"

make

echo "------------------------------"
echo "the esptool.py copy to bin "
echo "the alink.bin copy to bin "
echo "the bootloader_bin copy to bin"
echo "the partitions_two_ota.bin copy to bin"
echo "the alink.s copy to bin"
echo "--------------------------------"
cp esp-idf/components/esptool_py/esptool/esptool.py bin
cp ./build/alink.bin bin/
cp ./build/bootloader/bootloader.bin bin
cp esp-idf/components/esptool_py/esptool/esptool.py bin
cp  build/partitions_singleapp.bin bin
cp  build/partitions_two_ota.bin bin
xtensa-esp32-elf-objdump -S ./build/alink.elf > bin/alink.s

