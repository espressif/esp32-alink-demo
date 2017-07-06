#!/bin/bash
set -e

# install software
sudo apt-get install git wget make libncurses-dev flex bison gperf python python-serial

# Download the toolchain
mkdir -p ~/esp
cd ~/esp
wget https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-61-gab8375a-5.2.0.tar.gz
tar -xzf xtensa-esp32-elf-linux64-1.22.0-61-gab8375a-5.2.0.tar.gz

# Configure environment variables
echo 'export PATH=$PATH:$HOME/esp/xtensa-esp32-elf/bin' >> ~/.profile
source ~/.profile

