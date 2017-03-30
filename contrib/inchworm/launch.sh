procs=$1
fastafile=$2
dlsize=`expr 1024 \* 1024 \* 1024 \* 8`
acprun -n $procs -ndev udp -startermemsize-dl $dlsize ./MPIinchworm.py --reads $fastafile
