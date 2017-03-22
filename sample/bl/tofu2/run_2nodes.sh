#!/bin/bash -x
#PJM -L "rscunit=unit1,node=2:noncont,elapse=5:00"
#PJM -j
#PJM -S
#PJM --no-stging

fxlang_home=/opt/FJSVmxlang
export PATH=${fxlang_home}/bin:${PATH}
export LD_LIBRARY_PATH=/opt/FJSVpxtof/sparc64fx/lib64:${fxlang_home}/lib64

mpiexec -of-proc output ./a.out --acp-size-smem 67108864 --acp-size-smem-dl 67108864

