#!/bin/bash
set -e

export IDF_PATH=$(pwd)/esp-idf

echo -e "\033[32m----------------------"
echo "SDK_PATH: $IDF_PATH"
echo -e "----------------------\033[00m"

make -j8 flash

# copy bin
if [ ! -d "bin" ]; then
  mkdir bin
fi
echo -e "\033[32m----------------------"
echo "the esptool.py copy to bin "
echo "the alink.bin copy to bin "
echo "the bootloader_bin copy to bin"
echo "the partitions_two_ota.bin copy to bin"
echo "the alink.s copy to bin"
echo -e "----------------------\033[00m"
cp esp-idf/components/esptool_py/esptool/esptool.py bin
cp ./build/alink.bin bin/
cp ./build/bootloader/bootloader.bin bin
cp  build/partitions_two_ota.bin bin

xtensa-esp32-elf-objdump -S ./build/alink.elf > bin/alink.s &

make monitor

