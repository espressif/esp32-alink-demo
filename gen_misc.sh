#!/bin/bash
set -e

export IDF_PATH=$(pwd)/esp-idf

echo -e "\033[32m----------------------"
echo "SDK_PATH: $IDF_PATH"
echo -e "----------------------\033[00m"

PROJECT_NAME=alink

if [ ! -n "$1" ] ;then
    echo -e "\033[31m----------------------"
    echo "please enter the need to write the serial port number"
    echo "eg: ./gen_misc.sh 0"
    echo -e "----------------------\033[00m"
    exit 0
fi

alink_submodule=`ls -A components/esp32-alink|wc -w`
if [ "$alink_submodule" -le 1 ];then
    git submodule update --init --recursive
fi

if [ ! -d "bin" ]; then
  mkdir bin
fi

if [ ! -d "serial_log" ]; then
  mkdir serial_log
fi

if [ ! -d "device_id" ]; then
  mkdir device_id
fi

make clean
make -j8

# copy bin
echo -e "\033[32m----------------------"
echo "the esptool.py copy to bin "
echo "the alink.bin copy to bin "
echo "the bootloader_bin copy to bin"
echo "the partitions_two_ota.bin copy to bin"
echo "the alink.elf copy to bin"
echo -e "----------------------\033[00m"
cp esp-idf/components/esptool_py/esptool/esptool.py bin
cp build/bootloader/bootloader.bin bin
cp build/partitions_two_ota.bin bin
cp build/*.bin bin/
cp build/*.elf bin
cp build/*.map bin
cp tools/idf_monitor.py bin

cd bin

if [ ! -f "blank.bin" ]; then
    touch blank.bin
    for((i=0;i<4*1024;i++))
    do
        echo -e '\0377\c' >> 'blank.bin'
    done
fi

xtensa-esp32-elf-objdump -S $PROJECT_NAME.elf > $PROJECT_NAME.s &

mac=$(python esptool.py -b 115200 -p /dev/ttyUSB$1 read_mac)
mac_str=${mac##*MAC:}
mac_str=${mac_str//:/-}
log_file_name="../serial_log/"${mac_str:1:17}

sudo chmod 777 /dev/ttyUSB*

python esptool.py --chip esp32 --port /dev/ttyUSB$1 erase_flash

if [ -n "$2" ] ;then
    echo -e "\033[32m----------------------"
    echo "write device id to flash"
    echo $2 > 'device_id.bin'
    echo -e "----------------------\033[00m"

    python esptool.py --chip esp32 --port /dev/ttyUSB$1 --baud 1152000 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 bootloader.bin 0x10000 $PROJECT_NAME.bin 0x8000 partitions_two_ota.bin 0x310000 device_id.bin
else
    python esptool.py --chip esp32 --port /dev/ttyUSB$1 --baud 1152000 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 bootloader.bin 0x10000 $PROJECT_NAME.bin 0x8000 partitions_two_ota.bin
fi

python idf_monitor.py -b 115200 -p /dev/ttyUSB$1 -sf "${log_file_name}__ttyUSB$1.log" $PROJECT_NAME.elf
