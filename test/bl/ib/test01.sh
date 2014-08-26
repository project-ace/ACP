#!/bin/sh

hid=1
rank=0
np=$2
smsize=$3
port=$4
IP=`expr $hid + 1`
mport=`expr $port + 1`
dport=`expr $port + 2`

i=1
while [ $i -le $np ] ; then
do
    if [ $i -lt $np ] ; then
        ssh ace00$hid $1 $rank $np $smsize $mport $dport 192.168.1.$IP &> out$hid.log &
    else
        dport=`expr $port + 1`
        IP=1
        ssh ace00$hid $1 $rank $np $smsize $mport $dport 192.168.1.$IP &> out$hid.log
    fi
        rank=`expr $rank + 1`
        mport=`expr $mport + 1`
        hid=`expr $hid + 1`
        dport=`expr $dport + 1`
        IP=`expr $IP + 1`
        i=`expr $i + 1`
done

cat out*.log > result.log

