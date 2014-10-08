#!/bin/bash
#PBS -N 'test_acpbl_ip'
#PBS -q Q1
#PBS -j oe
#PBS -l nodes=15:ppn=1

#############################################################################
cd $PBS_O_WORKDIR
pwd
cat $PBS_NODEFILE

COMM_DIR=${HOME}/ACE/devel/test/ml/ch

# input 
# COMM=$COMM_DIR/acpch_ib_test0
# COMM=$COMM_DIR/acpch_ib_test1
#COMM=$COMM_DIR/acpch_ib_test2
# COMM=$COMM_DIR/acpch_ib_test3
COMM=$COMM_DIR/acpch_ib_test4

# # of process
np=2
ACP_MYRANK=$np
# # size of starter memory
smsize=1024
ACP_STARTER_MEMSIZE=$smsize
# start tcp/udp port 
port=10000
# port=44256

BASE_HOSTNAME=pc
BASE_IPADDR=192.168.1

hid=1
rank=0
IP=`expr $hid + 1`
mport=`expr $port + 1`
dport=`expr $port + 2`

i=1
while [ $i -le $np ]
do
    chid=`printf "%02d" $hid`
    if [ $i -lt $np ]
    then
        echo "ssh ${BASE_HOSTNAME}${chid} ACP_MYRANK=$rank ACP_LPORT=$mport ACP_RPORT=$dport ACP_RHOST=${BASE_IPADDR}.$IP ACP_STARTER_MEMSIZE=$smsize ACP_NUMPROCS=$np $COMM &"
        ssh ${BASE_HOSTNAME}${chid} ACP_MYRANK=$rank ACP_LPORT=$mport ACP_RPORT=$dport ACP_RHOST=${BASE_IPADDR}.$IP ACP_STARTER_MEMSIZE=$smsize ACP_NUMPROCS=$np $COMM &
    else
        dport=`expr $port + 1`
        IP=1
        echo "ssh ${BASE_HOSTNAME}${chid} ACP_MYRANK=$rank ACP_LPORT=$mport ACP_RPORT=$dport ACP_RHOST=${BASE_IPADDR}.$IP ACP_STARTER_MEMSIZE=$smsize ACP_NUMPROCS=$np $COMM"
	ssh ${BASE_HOSTNAME}${chid} ACP_MYRANK=$rank ACP_LPORT=$mport ACP_RPORT=$dport ACP_RHOST=${BASE_IPADDR}.$IP ACP_STARTER_MEMSIZE=$smsize ACP_NUMPROCS=$np $COMM 
    fi
    rank=`expr $rank + 1`
    mport=`expr $mport + 1`
    hid=`expr $hid + 1`
    dport=`expr $dport + 1`
    IP=`expr $IP + 1`
    i=`expr $i + 1`
done
