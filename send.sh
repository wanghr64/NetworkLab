#!/bin/bash

make -s clean && make -j 4

for i in {2..4}
do
scp ./son/son wanghr@192.168.56.$i:/home/wanghr/son
scp ./sip/sip wanghr@192.168.56.$i:/home/wanghr/sip
scp ./topology/topology.dat wanghr@192.168.56.$i:/home/wanghr/topology/topology.dat
done