#!/bin/bash
set -e
export IDF_PATH=$(pwd)/esp-idf
serial_port="/dev/ttyUSB0"
echo "----------------------"
echo "PLease Check IDF_PATH:" $IDF_PATH
echo "PLease check serial port:" $serial_port
echo "---------------------"

# sudo chmod 777 $serial_port
# make clean
make erase_flash flash -j8

# echo "------------------------------"
# echo "the esptool.py copy to bin "
# echo "the alink.bin copy to bin "
# echo "the bootloader_bin copy to bin"
# echo "the partitions_two_ota.bin copy to bin"
# echo "the alink.s copy to bin"
# echo "--------------------------------"
# cp esp-idf/components/esptool_py/esptool/esptool.py bin
# cp ./build/alink.bin bin/
# cp ./build/bootloader/bootloader.bin bin
# cp  build/partitions_two_ota.bin bin
# xtensa-esp32-elf-objdump -S ./build/alink.elf > bin/alink.s
minicom -c on
