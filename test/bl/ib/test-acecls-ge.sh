#!/bin/bash
#PBS -N 'test_acpbl_ip'
#PBS -q Q1
#PBS -j oe
#PBS -l nodes=2:ppn=1

#############################################################################
cd $PBS_O_WORKDIR
pwd
cat $PBS_NODEFILE

COMM_DIR=${HOME}/ACE/devel/test/bl/ib

# input 
# COMM=$COMM_DIR/acpbl
COMM=$COMM_DIR/acpbl_ohandle
# COMM=$COMM_DIR/acpbl_rm
# COMM=$COMM_DIR/acp_bw
# COMM=$COMM_DIR/acpbl_atomic
# COMM=$COMM_DIR/acpbl_atomic8
# COMM=$COMM_DIR/acpbl_rr
# COMM=$COMM_DIR/acpbl_rr2
# COMM=$COMM_DIR/acpbl_reset
# COMM=$COMM_DIR/acp_tp
# COMM=$COMM_DIR/acpbl_ch_copy
# COMM=$COMM_DIR/rring

# # of process
np=2
ACP_MYRANK=$np
# # size of starter memory
smsize=10240000
ACP_STARTER_MEMSIZE=$smsize
# start tcp/udp port 
port=10000
#port=44256

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
