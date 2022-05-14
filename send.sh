#!/bin/bash

for i in {1..4}
do
scp ./son/son root@192.168.20.$i:/root
scp ./sip/sip root@192.168.20.$i:/root
done