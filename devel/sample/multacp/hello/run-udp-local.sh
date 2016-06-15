#!/bin/sh

####################################################
NODES=1
NCORES_NODE=6
NPROCS=`expr $NODES \* $NCORES_NODE`
NPROCS=6
INPUT=input6_udp

DEV=UDP

####################################################
macprun -n $NPROCS -net $DEV $INPUT
