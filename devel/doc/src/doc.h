/**
   @mainpage
   
   @section intro Introduction
   
   @subsection about About This Document
   
   This document introduces the specifications of the interfaces of 
   Advanced Communication Primitives (ACP) Library. 
   This library is designed to enable applications with sufficient 
   inherent parallelism to achieve high scalability up to exa-scale 
   computing systems, where the number of processes is expected to 
   be more than a million.
   
   @subsection motivation Motivation
   
   As the performance of environments for high-performance computing (HPC) 
   is approaching from peta to exa, the number of cores in one node is 
   increasing, while the amount of memory per node is expected to remain 
   almost the same. Therefore, reducing memory consumption has become 
   an important issue for achieving sustained scalability toward 
   exa-scale computers.
   
   Especially, communication middleware on those environments must be 
   carefully designed to achieve high memory efficiency. 
   There is always a trade-off between the memory consumption and the 
   performance of communication. To avoid conflicts of resources and 
   achieve high performance, sufficient amount of buffers should be allocated. 
   Therefore, appropriate control of the allocation and deallocation of 
   buffers is a key to enable memory-efficient communications.
   
   @subsection approach Approach
   
   To minimize the requirements of memory consumption at the initialization, 
   ACP Library provides RDMA model as the basic communication layer. On 
   interconnect networks where RDMA is supported as a fundamental facility, 
   this layer can be implemented with minimal memory consumption and overhead.
   
   Since programming with RDMA needs detailed operations such as memory 
   registrations, address exchanges and synchronizations, ACP also prepares 
   some sets of programmer-friendly interfaces as the middle layer. 
   To enable the library to consume just-enough amount of memory, each 
   interface of the middle layer requires explicit allocation of the 
   memory region before using it. This region can be explicitly 
   de-allocated so that the memory region can be reused for other purposes.
   
   Each of the interfaces of this middle layer is primitive and independent. 
   For example, the channel interface in this layer only supports 
   one-directional and in-order data transfer between a pair of processes. 
   This helps to minimize the memory consumption and overhead in the 
   implementation. Also, the independent interfaces enable precise control 
   of the allocation and de-allocation of memory regions for them. 
   At this point, interfaces of channels, vectors, lists and memory 
   allocation are defined in ACP. There are plans for other interfaces 
   such as group communications, deques, maps, sets and counters.
   
   @subsection terminology Terminology

   term          | description
   ----------------|------
   process        |A unit of program execution by OS.
   task           |A unit of parallel program execution including multiple processes.
   rank           |A number to distinguish process in a task.
   ACE project    |Advanced Communication for Exa project. It develops a communication library with low memory consumption and low overhead.
   ACP library    |Advanced Communication Primitives library. A library that has been developed in the ACE project.
   GMA            |Global Memory Access. Direct access to the memory of a remote process in a task.
   global memory  |Memory region to which remote processes can establish GMAs.
   starter memory |A region of global memory that is prepared in each rank at the initialization.
   global address |64bit address space shared by all of the processes in a task.
   address translation key |An identifier used for translating logical address to global address.
   color          |A number to distinguish the path for transferring data with GMA.
   channel        |Virtual path to transfer messages between a pair of processes.
   vector         |One-dimensional array.
   list           |Bi-directional list.
   iterator       |An index to refer to an element in a data structure.


   @section overview Overview

   @subsection fundamentalDesign Fundamental Design

   @subsubsection execModel Execution Model

   The execution model of ACP is a single program multiple data (SPMD) model. 
   Therefore, each process in a task executes the same program. Processes 
   are distinguished by ranks. Ranks are mapped to the processes at the 
   initialization. ACP also supports re-mapping ranks during the execution.

   @subsubsection commModels Communication Models

   As written in the previous section, the basic communication model of ACP 
   is RDMA. However, in this model, programmers need to write codes for 
   memory registration, address exchange and appropriate synchronization 
   before establishing data transfer. Therefore, ACP also supports additional 
   two categories of interfaces as the middle layer, the communication 
   patterns and the data structures. The communication pattern interfaces 
   support explicit transfers of data on the local memory of each process. 
   Currently, ACP provides one interface, channel, as this category of 
   interfaces.

   On the other hand, the data structure interface supports implicit data 
   transfer via the operations on dynamic distributed data structures. 
   At this point, vector and list are supported in this category. The heart 
   of this category is an asynchronous global memory allocator that allocates 
   a segment of global memory involving no processor at the provider-side. 
   The design concept of this category derives from the C/C++ languages 
   and is more straightforward than the novel design concept of the basic layer.

   As mentioned previously, memory efficiency is the target property in the 
   design of ACP. Towards this target, each interface in the middle layer is 
   designed to support a primitive facility so that it can be implemented 
   with minimal memory consumption and overhead. In addition to that, to 
   enable just-enough memory consumption, each interface of the middle layer 
   consists of functions for allocation and de-allocation. Programs must 
   explicitly call the allocation function of the interface before 
   establishing data-transfer operations in it. This function allocates a 
   region of memory to be used by a set of processes that participate in 
   the corresponding communication pattern or the data structure. 
   Eventually the information required for establishing communications on 
   the interface is exchanged until the completion of the first data 
   transfer with it. On the other hand, the de-allocation function frees 
   the memory regions allocated for the interface.

   In a program, the same set of processes can call the allocation function 
   of the interface multiple times. This will prepare independent memory 
   region for each call. This policy can cause redundant allocation of 
   memory region. However, sharing resources among multiple calls of 
   allocation can complicates the mechanisms for handling memory regions 
   and causes higher memory consumptions and overheads.

   To establish communications that are not supported in the middle layer, 
   the programmers can use the interfaces of the basic RDMA operations of ACP. 
   It consists of two categories of interfaces, the global memory management 
   (GMM) and the global memory access (GMA). In this model, any local memory 
   regions can be mapped to the global address space shared among processes. 
   ACP uses 64bit unsigned integer as the global address that specifies the 
   position in the space. As GMA, the remote copy operation and some atomic 
   operations are supported.


   @subsection interfaces Interfaces

   @subsubsection infrastructure Infrastructure

   Each process starts its execution as a part of an ACP program by calling 
   the initialization function, _acp_init_, and ends by calling finalization 
   function, __acp_finalize__. ACP also prepares a function for aborting the 
   program, __acp_abort__, in case of an error.

   The reset function, __acp_reset__, remaps ranks to the processes as 
   specified by its argument. There are functions to query current rank of 
   the process, __acp_rank__, and the total number of processes, 
   __acp_procs__.

   Processes are implicitly synchronized at the initialization, reset and 
   finalization. There is also a synchronization function, __acp_sync__, to 
   explicitly synchronize all of the processes. This function returns after 
   determining arrivals of all processes.

   @subsubsection channel Channel

   This interface provides a virtual pass for message passing from one 
   process to another. The data transfer supported by it is single direction 
   and in-order. Therefore, the implementation of it can be simple and 
   efficient. Before establishing data transfer with the interface, both the 
   sender process and the receiver process must call the allocation function 
   to establish a channel between them. If a channel has become useless, 
   the program can free the channel by calling the de-allocation function 
   at both sides.

   @image html fig-channel.png "schematic picture of Channel interface" width=15cm
   @image latex fig-channel.pdf "schematic picture of Channel interface" width=15cm

   The allocation function, __acp_create_ch__, of this interface allocates 
   a region of memory according to the role of the process that invoked the 
   function. After that, it starts exchanging the information about the 
   region with another peer of the channel to establish the connection of 
   the channel. Then, it returns a handle of the channel, with the data type 
   of __acp_ch_t__, without waiting for the completion of the connection. 
   The library implicitly progresses the establishment of the connection.

   The non-blocking functions for sending and receiving messages, 
   __acp_nbsend_ch__ and __acp_nbrecv_ch__, on a channel can be invoked 
   before the completion of its connection. Each of these functions stores 
   the information about the invocation in the request queue of the channel, 
   and returns a request handle with the data type of __acp_request_t__. 
   It starts transferring messages after it has completed the connection. 
   The wait function, __acp_wait_ch__, wait for the completion of the request.

   The non-blocking de-allocation function, __acp_free_ch__, starts freeing 
   memory regions of the channel specified by the handle. The behavior of 
   the functions of send and receive on the channel that has been specified 
   for this function is not defined.

   @subsubsection vector Vector

   The vector interface provides functions to manipulate dynamic array. 
   Each element of a vector can be manipulated by an iterator. An iterator is 
   valid only at the process that retrieved it. Iterators will be invalid after 
   modification of the vector by: __acp_assign_vector__, __acp_assign_range_vector__, 
   __acp_clear_vector__, __acp_destroy_vector__, __acp_erase_vector__, 
   __acp_erase_range_vector__, __acp_insert_vector__, __acp_insert_range_vector__
   and __acp_swap_vector__.

   @subsubsection deque Deque

   The deque interface provides functions for establishing the data 
   structure and functions of doule-ended queues.Each element of a deque can
   be manipulated by an iterator. An iterator is valid only at the process that 
   retrieved it. Iterators will be invalid after modification of the deque by: 
   __acp_assign_deque__, __acp_assign_range_deque__, __acp_clear_deque__, 
   __acp_destroy_deque__, __acp_erase_deque__, __acp_erase_range_deque__, 
   __acp_insert_deque__, __acp_insert_range_deque__ and __acp_swap_deque__. 

   @subsubsection list List

   The list interface provides functions for establishing the data structure and 
   algorithms of bidirectional linked list. Each element of a list can be manipulated by 
   an iterator. An iterator is valid only at the process that retrieved it. Iterators will 
   be invalid after modification of the list by: __acp_assign_list__, 
   __acp assign_range_list__, __acp_clear_list__, __acp_destroy_list__, 
   __acp_erase_list__, __acp_erase_range_list__, __acp_insert_list__, 
   __acp_insert_range_list__, __acp_merge_list__, __acp_remove_list__, 
   __acp_reverse_list__, __acp_splice_list__, __acp_swap_list__ 
   and __acp_unique_list__.

   @subsubsection map Map

   The map interface provides the data structure and algorithms of associative arrays. 
   The type for referencing maps is __acp_map_t__, the type for the iterator of maps is
   __acp_map_it_t__, and the type for returning result of finding element in a map is 
   __acp_map_ib_t__. Maps are created via the constructor function, acp_create_map. 
   This function prepares an empty map distributed among a group of processes. 
   As an argument, it accepts the number of processes, the array of ranks of the processes 
   in the group, the number of slots and the rank to place the control structure of the map. 
   Elements can be added to a map via the function __acp_insert_map__. 
   In addition to the map to insert, this function accepts the key and the value of the element, 
   as an argument. Each of the key and the value are specified by a pair of the address 
   and the size in byte. The rank to place the element to be inserted is decided according to 
   the hash value calculated from the key. __acp_find_map__ searches the key in a map. 
   It returns the value true or false according to the result, and the iterator of the 
   element with the key if it has been found.

   @subsubsection malloc Malloc

   The malloc interface provides the functions for asynchronous allocation 
   and de-allocation of global memory. It is the same as that of the 
   asynchronous global heap that designs a memory pool to be accessible via 
   RDMA as well as via processor instructions. This interface consists of 
   the __acp_malloc__ function that allocates a segment of global memory 
   from specified process by a rank number. A global memory segment 
   allocated by this function can be freed by the __acp_free__ function.

   @subsubsection gmmSSSec Global Memory Management

   The basic layer provides global address space shared among all of the 
   processes. This space is addressed by 64bit unsigned integer, 
   so that it can be directly handles by the atomic operations of CPUs 
   and devices.

   Any local memory space of any process can be mapped to this space via 
   the registration function, __acp_register_memory__, of this layer. 
   It returns a key with the data type of __acp_atkey_t__. There is also a 
   de-registration function, __acp_unregister_memory__, to unregister the 
   region of the specified key. Global address in a registered region can 
   be retrieved by a query function, __acp_query_ga__. It returns the 
   address with the data type of __acp_ga_t__.

   At the registration, in addition to the start address and the size of 
   the local region to be mapped, the color of the global address can be 
   specified. Color is used for specifying device in the node to be used 
   for making remote access to the region. There is a function, 
   __acp_colors__, to query the maximum number of colors available on the 
   current system. At the initialization, a special region, called starter 
   memory, is allocated on each process. Each of the starter memories of 
   the processes is registered to the global address space, and there is a 
   function, __acp_query_starter_ga__, to query for the global address of 
   the starter memory of the specified rank. This region is mainly used for 
   exchanging global addresses before performing RDMA operations on the 
   global address space.

   @subsubsection gmaSSSec Global Memory Access

   The RDMA operations on the global memory space of the basic layer is 
   called global memory access (GMA). ACP prepares GMA functions, such 
   as __acp_copy__, __acp_add4__, __acp_add8__, __acp_cas4__ and so on, 
   to perform copy and atomic operations on the global memory space. 
   Unlike other communication libraries, both of the source and the 
   destination of copy function can be remote.

   @image html fig-gma.png "schematic picture of Global Memory Access interface" width=15cm
   @image latex fig-gma.pdf "schematic picture of Global Memory Access interface" width=15cm

   All of the GMA functions are non-blocking. Therefore, each of them 
   returns a GMA handle with the data type of __acp_handle_t__ to wait 
   for the completion. Each GMA function has an argument 'order' to specify 
   the condition for starting the operation. If a GMA handle is specified 
   for this argument, it will wait for the completion of the GMA of the 
   handle before starting its access. If __ACP_HANDLE_NULL__ is specified 
   as the order, it starts the access immediately. If __ACP_HANDLE_ALL__ is 
   specified, it will wait for the completions of all of the previously 
   invoked GMAs. If __ACP_HANDLE_CONT__ is specified as the order, and if 
   the GMA that is invoked immediately before this one has the same source 
   rank, target rank, source color and target color, it can start its access 
   with the same condition of the previous one, as far as it will not 
   overtake the order. If there is any difference in those parameters, 
   the behavior is same as the case with __ACP_HANDLE_ALL__. There can be an 
   implementation that always performs the same way as the case with 
   __ACP_HANDLE_ALL__, even if __ACP_HANDLE_CONT__ is specified. The 
   completion function, __acp_complete__, waits for the completion of the 
   GMA specified by the handle. The completions of GMA functions are ensured 
   in-order. This means that the completion of one GMA function ensures the 
   completion of all of the other GMA functions that have been invoked 
   earlier than it on the process. There is also a function, __acp_inquire__, 
   which check the completions of the GMA specified by the handle and the 
   GMAs that have been invoked before it.

   @section usage Usage
   @subsection installation Installation

   The simplest way to install ACP library is typically a combination of running 
   "configure" and "make". Execute the following commands to install the ACP library system
   from within the directory at the top of the tree:

   <tt>shell$ ./configure --prefix=/where/to/install\n
       [...lots of output...]\n
   shell$ make all install
   </tt>

   If you need special access to install, then you can execute "make all" as a user with write
   permissions in the build tree, and a separate "make install" as a user with write permissions
   to the install tree.
   
   See INSTALL for more details.

   @subsection compilation Compilation

   ACP provides "wrapper" compilers that should be used for compiling ACP applications:
   \arg C:	\c acpcc, \c acpgcc
   \arg C++:	\c acpc++, \c acpcxx
   \arg Fortran:	\c acpgc (not provided yet)

   For example:

   <tt>
   shell$ acpcc hello_world_acp.c -o hello_world_acp -g\n
   shell$
   </tt>

   All the wrapper compilers do is add a variety of compiler and linker flags to the command
   line and then invoke a back-end compiler. To be specific: the wrapper compilers do not parse
   source code at all; they are solely command-line manipulators, and have nothing to do with
   the actual compilation or linking of programs. The end result is an ACP executable that is
   properly linked to all the relevant libraries.

   Customizing the behavior of the wrapper compilers is possible 
   (e.g., changing the compiler [not recommended] or specifying additional compiler/linker flags).

   @subsection execution Execution

   ACP library supports acprun program to launch ACP appliations. For example:

   <tt>
   shell$ acprun -np 2 hello_world_acp
   </tt>

   This launches two \c hello_world_acp applications on localhost. 
   Option \c "-np 2" is same as long option \c "--acp-nprocs=2". 
   Please note that \c "-np nprocs" has to be 'first' option of acprun command. 
   There are no such limitations for long option.

   You can specify a \c --acp-nodefile=nodefile option to indicate hostnames on which 
   \c hello_world_acp command will be launched.

   If you want launch two processes on pc01 and pc02, use following command:

   <tt>
   shell$ acprun -np 2 --acp-nodefile=nodefile hello_world_acp\n
   </tt>
   with the following nodefile:\n
   <tt>
   ---------------------------------------------------------------------------\n
   pc01\n
   pc01\n
   pc02\n
   pc02\n
   ---------------------------------------------------------------------------\n
   </tt>

   To specify network device, for example infiniband (or udp, tofu), use 
   \c --acp-envent=ib (or udp, tofu) option as follows:

   <tt>
   shell$ acprun -np 2 --acp-nodefile=nodefile --acp-nodefile=ib hello_world_acp
   </tt>

   If you want control starter memory size of ACP basic layer library, use 
   \c --acp-startermemsize=size (byte unit) option as follows:
   
   <tt>
   shell$ acprun -np 2 --acp-nodefile=nodefile --acp-nodefile=ib --acp-startermemsize=4096 hello_world_acp
   </tt>

   Notice:
   \arg Other \c "--acp-*" options are parsed as errors in regard to invalid option, even though that option is used as user program option.
   \arg Tofu network device may not supported in this version.

*/
/**
   @file doc.h
   
   Document file.
*/
