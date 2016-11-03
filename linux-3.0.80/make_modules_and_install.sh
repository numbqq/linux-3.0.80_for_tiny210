#!/bin/bash

make modules

make modules_install ARCH=arm INSTALL_MOD_PATH=/home/nick/nfs/rootfs

