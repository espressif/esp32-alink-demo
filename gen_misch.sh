#!/bin/bash
# set -e
export IDF_PATH=$(pwd)/esp-idf
echo "----------------------"
echo "PLease Check IDF_PATH:" $IDF_PATH
echo "---------------------"

var="components/esp-alink-v1.1/component.mk"
grep -q "CONFIG_ALINK_V11_ENABLE=y" sdkconfig
if [[ $? -eq 0 ]]; then
    echo "ALINK version: alink_v1.1"
    if [[ -e $var.bak ]]; then
        mv -f $var.bak  $var
    fi
else
    if [[ -e $var ]]; then
        mv -f $var  $var.bak
    fi
fi

var="components/esp-alink-v2.0/component.mk"
grep -q "CONFIG_ALINK_V20_ENABLE=y" sdkconfig
if [[ $? -eq 0 ]]; then
    echo "ALINK version: alink_v2.0"
    if [[ -e $var.bak ]]; then
        mv -f $var.bak $var
    fi
else
    if [[ -e $var ]]; then
        mv -f $var $var.bak
    fi
fi

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
cp  build/partitions_two_ota.bin bin
xtensa-esp32-elf-objdump -S ./build/alink.elf > bin/alink.s
