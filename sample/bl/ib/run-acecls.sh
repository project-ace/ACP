#!/bin/sh
#PBS -N 'test_acpbl_ip'
#PBS -q Q1
#PBS -j oe
#PBS -l nodes=16:ppn=1

#############################################################################
cd $PBS_O_WORKDIR
pwd
cat $PBS_NODEFILE

rm *.dat

./run-acecls-exec.sh 16 1024 10000
