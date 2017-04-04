#!/bin/bash

rm bbb.dat

for i in $(seq -w 001 360) ; do
    echo -n $i | tee -a bbb.dat
    for num in $(seq 0 255) ; do
        printf "%b" "\x$(echo "obase=16;ibase=10;$num" | bc)" >>bbb.dat
    done
done

