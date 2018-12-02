#!/bin/bash

make
sudo insmod fifodev.ko
sudo mknod -m 666 /dev/fifodev c 248 0
cd ../FifoTest
gnome-terminal -e "./fifotest -f /dev/fifodev -r"
echo "Aqui se escribe"
./fifotest -f /dev/fifodev -s




