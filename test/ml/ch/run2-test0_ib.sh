#!/bin/bash
export ACP_MYRANK=0
export ACP_NUMPROCS=2
export ACP_STARTER_MEMSIZE=1024
export ACP_RHOST=192.168.1.1
export ACP_LPORT=10001
export ACP_RPORT=10002
./acpch_ib_test0 &> test0_0.log &  
export ACP_MYRANK=1
export ACP_NUMPROCS=2
export ACP_STARTER_MEMSIZE=1024
export ACP_RHOST=192.168.1.1
export ACP_LPORT=10002
export ACP_RPORT=10001
./acpch_ib_test0 &> test0_1.log &

