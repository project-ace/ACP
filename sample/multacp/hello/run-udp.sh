#!/bin/sh
#PBS -N 'qn_out'
#PBS -q Q1
#PBS -j oe
#PBS -l nodes=6:ppn=1:D

####################################################
NODES=6
NCORES_NODE=1
NPROCS=`expr $NODES \* $NCORES_NODE`
NPROCS=6
INPUT=input6_udp

DEV=UDP

#torque environment variables
DIR=$PBS_O_WORKDIR
NODEFILE=$PBS_NODEFILE

cd $DIR
echo '# NODES:'
cat $NODEFILE
echo '# -------'

####################################################
macprun -n $NPROCS -machinefile $PBS_NODEFILE -net $DEV $INPUT
