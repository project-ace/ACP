ACP Library on Tofu interconnect
===========

 0. Overview
 1. Installation
 2. Compiling user ACP Applications
 3. Running user ACP Applications

0. Overview
-----------
Advanced Communication Primitives (ACP) Library is designed to enable applications 
with sufficient inherent parallelism to achieve high scalability up to exa-scale 
computing systems, where the number of processes is expected to be more than a million.

See http://ace-project.kyushu-u.ac.jp/index.html for more details.


1. Installation
---------------
 A. Install acpbl_tofu_sys library
  Place following file and symbolic links to a local directory accessible from
  compute nodes.

    libacpbl_tofu_sys.so
    libacpbl_tofu_sys.so.#
    libacpbl_tofu_sys.so.#.#.#

  To obtain these files or to use pre-installed files, contact your system
  administrator.

  Confirm that the path is specifid in both of the environment variables
  LIBRARY_PATH and LD_LIBRARY_PATH.

  For Tofu interconnect 2, install or use acpbl_tofu2_sys library instead of
  acpbl_tofu_sys library.


 B. Install ACP
  Add "--with-tofu" of "--with-tofu2" option to the configure command.

  shell$ ./configure --with-tofu --prefix=/where/to/install
  [...lots of output...]
  shell$ make all install


2. Compiling user ACP Applications
--------------------------------
ACP provides "wrapper" compilers that should be used for
corss-compiling ACP applications on a login node:

C:          acpfccpx
C++:        acpFCCpx
Fortran:    acpfrtgx (not provided yet)

For example:

  shell$ acpfccpx hello_world_acp.c -o hello_world_acp -g
  shell$

All the wrapper compilers do is add a variety of compiler and linker
flags to the command line and then invoke a back-end compiler.  To be
specific: the wrapper compilers do not parse source code at all; they
are solely command-line manipulators, and have nothing to do with the
actual compilation or linking of programs.  The end result is an ACP
executable that is properly linked to all the relevant libraries.

Customizing the behavior of the wrapper compilers is possible (e.g.,
changing the compiler [not recommended] or specifying additional
compiler/linker flags);


3. Running user ACP Applications
--------------------------------
To run your user application, use mpiexec command in a job script or
on a terminal of interactive job.

  mpiexec ./a.out

The job script must specify the properties of the parallel execution
such as the number of nodes in the same way as an MPI task.

"--acp-size-smem" option specifies the starter memory size (byte unit) of ACP
basic layar as follows:

  mpiexec ./a.out --acp-size-smem 67108864

To use ACP data library functions such as acp_malloc(), specify the maximum
heap size with "-smemsize-dl size" or "--acp-startermemsize-dl=size" (byte unit)
option as follows:

  mpiexec ./a.out --acp-size-smem-dl 268435456

Heap memory will be allocated dynamically at initialization.

Options:
      --acp-size-smem           starter memory size (user region)
      --acp-size-smem-cl        starter memory size (comm. library)
      --acp-size-smem-dl        starter memory size (data library)
      --acp-malloc-hook-small   malloc hook flag [0|1] for small size
      --acp-malloc-hook-large   malloc hook flag [0|1] for large size
      --acp-malloc-hook-huge    malloc hook flag [0|1] for small size
      --acp-malloc-hook-low     mallok hook low threshold
      --acp-malloc-hook-high    mallok hook high threshold

===========================================================================

Copyright (c) 2014      Kyushu University 
Copyright (c) 2014      Institute of Systems, Information Technologies 
                        and Nanotechnologies 2014
Copyright (c) 2014      FUJITSU LIMITED


