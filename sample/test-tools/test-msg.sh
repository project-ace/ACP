#!/bin/bash
#PBS -q Q1
#PBS -j oe
#PBS -l nodes=8:ppn=1

#############################################################################
cd $PBS_O_WORKDIR

ACP_DIR=/usr/local/acp

$ACP_DIR/bin/acprun -np 8 --acp-nodefile=$PBS_NODEFILE ./test-msg
