#!/bin/sh

COMM_DIR=${HOME}/ACE/devel/test/bl/ib
COMM=$COMM_DIR/acpbl_ohandle
BASE_HOSTNAME=pc
BASE_IPADDR=192.168.1

hid=1
rank=0
np=$1
smsize=$2
port=$3
IP=`expr $hid + 1`
mport=`expr $port + 1`
dport=`expr $port + 2`

i=1
while [ $i -le $np ]
do
    chid=`printf "%02d" $hid`
    if [ $i -lt $np ]
    then
        echo "ssh ${BASE_HOSTNAME}${chid} $COMM $rank $np $smsize $mport $dport ${BASE_IPADDR}.$IP &> out${chid}.log &"
        ssh ${BASE_HOSTNAME}${chid} $COMM $rank $np $smsize $mport $dport ${BASE_IPADDR}.$IP &> out${chid}.log &
    else
        dport=`expr $port + 1`
        IP=1
        echo "ssh ${BASE_HOSTNAME}${chid} $COMM $rank $np $smsize $mport $dport ${BASE_IPADDR}.$IP &> out${chid}.log"
        ssh ${BASE_HOSTNAME}${chid} $COMM $rank $np $smsize $mport $dport ${BASE_IPADDR}.$IP &> out${chid}.log
    fi
    rank=`expr $rank + 1`
    mport=`expr $mport + 1`
    hid=`expr $hid + 1`
    dport=`expr $dport + 1`
    IP=`expr $IP + 1`
    i=`expr $i + 1`
done

sleep 3
cat out*.log > result.log

