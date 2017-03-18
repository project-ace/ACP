#!/bin/bash -x
#PJM -L "rscunit=unit1,node=2:noncont,elapse=5:00"
#PJM -j
#PJM -S

fxlang_home=/opt/FJSVmxlang
export PATH=${fxlang_home}/bin:${PATH}
export LD_LIBRARY_PATH=/gfs14/mpi/aji/work/acp/devel/src/bl/tofu2:${fxlang_home}/lib64

HZ=1975

mpiexec ./a.out dummy dummy dummy 67108864 dummy dummy dummy

#HZ=2200
#
#mpiexec -of-proc malloc_${HZ}_out.${PJM_JOBID} ./malloc_${HZ} dummy dummy dummy 67108864 dummy dummy dummy
#mpiexec -of-proc map_${HZ}_out.${PJM_JOBID}    ./map_${HZ}    dummy dummy dummy 67108864 dummy dummy dummy
