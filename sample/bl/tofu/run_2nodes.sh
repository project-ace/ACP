#!/bin/bash -x
#PJM -L "rscunit=unit1,rscgrp=grp1,node=2,elapse=5:00"
#PJM -j
#PJM -S
#PJM --no-stging

fxlang_home=/opt/FJSVfxlang/1.2.1
export PATH=${fxlang_home}/bin:${PATH}
export LD_LIBRARY_PATH=/opt/FJSVpxtof/sparc64fx/lib64:${fxlang_home}/lib64:.

mpiexec -of-proc output ./a.out --acp-size-smem 67108864 --acp-size-smem-dl 67108864
