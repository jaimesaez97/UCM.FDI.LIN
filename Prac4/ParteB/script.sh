#!/bin/bash

make
sudo insmod fifoproc.ko
cd ../FifoTest
gnome-terminal -e "./fifotest -f /prsoc/fifoproc -r"
echo "Aqui se escribe"
./fifotest -f /proc/fifoproc -s




