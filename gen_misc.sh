#!/bin/bash
set -e

export IDF_PATH=$(pwd)/esp-idf

echo -e "\033[32m----------------------"
echo "SDK_PATH: $IDF_PATH"
echo -e "----------------------\033[00m"

if [ ! -d "bin" ]; then
  mkdir bin
fi

if [ ! -d "alink_log" ]; then
  mkdir alink_log
fi

if [ ! -d "device_id" ]; then
  mkdir device_id
fi

# make clean
make -j8
xtensa-esp32-elf-objdump -S ./build/alink.elf > bin/alink.s &

# copy bin
echo -e "\033[32m----------------------"
echo "the esptool.py copy to bin "
echo "the alink.bin copy to bin "
echo "the bootloader_bin copy to bin"
echo "the partitions_two_ota.bin copy to bin"
echo "the alink.elf copy to bin"
echo -e "----------------------\033[00m"
cp esp-idf/components/esptool_py/esptool/esptool.py bin
cp ./build/bootloader/bootloader.bin bin
cp  build/partitions_two_ota.bin bin
cp ./build/*.bin bin/
cp  build/*.elf bin
cp  build/*.map bin

if [ ! -n "$1" ] ;then
    log_file_name="alink_log/"$(date "+%Y-%m-%d_%H-%M-%S")".log"
    make erase_flash flash monitor | tee $log_file_name
else
    cd bin
    if [ ! -f "blank.bin" ]; then
        for((i=0;i<4*1024;i++))
        do
            echo -e '\0377\c' >> 'blank.bin'
        done
    fi

    python esptool.py --chip esp32 --port /dev/ttyUSB$1 erase_flash
    python esptool.py --chip esp32 --port /dev/ttyUSB$1 --baud 1152000 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 bootloader.bin 0x10000 *alink*.bin 0x8000 partitions_two_ota.bin
    cd -

    log_file_name="alink_log/ttyUSB$1_"$(date "+%Y-%m-%d_%H-%M-%S")".log"
    # pkill minicom
    minicom -D /dev/ttyUSB$1 -c on -C "$log_file_name"
fi
