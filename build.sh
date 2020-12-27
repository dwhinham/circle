#!/bin/bash

set -e

make -C addon/fatfs clean
make -C addon/SDCard clean
make -C app/sdcard-test clean
./makeall clean

./makeall -j
make -C addon/fatfs -j
make -C addon/SDCard -j
make -C app/sdcard-test -j
