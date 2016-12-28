///#ifndef __INCLUDE_DEFAULT_INPUT__
///#define __INCLUDE_DEFAULT_INPUT__

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
#ifdef __INSIDE_ACPBL_INPUT_H__

#define IR_PORTFILE         0
#define IR_OFFSETRANK       1
#define IR_MYRANK           2
#define IR_NPROCS           3
#define IR_LPORT            4
#define IR_RPORT            5
#define IR_RHOST            6
#define IR_TASKID           7
#define IR_SZSMEM_BL        8
#define IR_SZSMEM_CL        9
#define IR_SZSMEM_DL       10
#define IR_ETHSPEED        11
#define _NIR_              12

#endif ///__INSIDE_ACPBL_INPUT_H__

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
#ifdef __INSIDE_ACPBL_INPUT_C__

static acpbl_default_option_t default_opts[] = {
    /// *name         type,             u_default, d_default,      s_default,                 u_min,                  u_max, d_min,  d_max
    ///
    { "portfile",   STRING, 0xffffffffffffffffLLU,     0.0e0,     "portfile", 0xffffffffffffffffLLU,  0xffffffffffffffffLLU, -0.1e0, 0.1e0 }, ///  0, IR_PORTFILE 0
    { "offsetrank",   UINT,                     0,     0.0e0,            NULL,                     0,              10000000, -0.1e0, 0.1e0 }, ///  1, IR_OFFSETRANK
    ///
    { "myrank",       UINT,                     0,     0.0e0,            NULL,                     0,              10000000, -0.1e0, 0.1e0 }, ///  2, IR_MYRANK
    { "nprocs",       UINT,                     1,     0.0e0,            NULL,                     0,              10000000, -0.1e0, 0.1e0 }, ///  3, IR_NPROCS
    { "lport",        UINT,                 44256,     0.0e0,            NULL,                 44256,                 61000, -0.1e0, 0.1e0 }, ///  4, IR_LPORT
    { "rport",        UINT,                 44256,     0.0e0,            NULL,                 44256,                 61000, -0.1e0, 0.1e0 }, ///  5, IR_RPORT
    { "rhost",      STRING, 0xffffffffffffffffLLU,     0.0e0,     "127.0.0.1", 0xffffffffffffffffLLU, 0xffffffffffffffffLLU, -0.1e0, 0.1e0 }, ///  6, IR_RHOST
    ///
    { "taskid",       UINT,                     1,     0.0e0,            NULL,                     0,              10000000, -0.1e0, 0.1e0 }, ///  7, IR_TASKID
    { "szsmem",       UINT,                 10240,     0.0e0,            NULL,                     0,           10000000000, -0.1e0, 0.1e0 }, ///  8, IR_SZSMEM_BL
    { "szsmemcl",     UINT,                 10240,     0.0e0,            NULL,                     0,           10000000000, -0.1e0, 0.1e0 }, ///  9, IR_SZSMEM_CL
    { "szsmemdl",     UINT,                 10240,     0.0e0,            NULL,                     0,           10000000000, -0.1e0, 0.1e0 }, /// 10, IR_SZSMEM_DL
    { "ethspeed",     UINT,                  1000,     0.0e0,            NULL,                     1,              10000000, -0.1e0, 0.1e0 }, /// 11, IR_ETHSPEED
    { NULL,     0xffffffff, 0xffffffffffffffffLLU,     0.0e0,            NULL, 0xffffffffffffffffLLU, 0xffffffffffffffffLLU, -0.1e0, 0.1e0 }
} ;

static acpbl_option_t long_options[] = {
    /// *name                  has_arg,            *flag, val
    { "--acp-portfile",        _required_argument_, NULL, IR_PORTFILE   }, /// 0
    { "--acp-offsetrank",      _required_argument_, NULL, IR_OFFSETRANK }, /// 1
    ///
    { "--acp-myrank",          _required_argument_, NULL, IR_MYRANK     }, /// 2
    { "--acp-nprocs",          _required_argument_, NULL, IR_NPROCS     }, /// 3
    { "--acp-port-local",      _required_argument_, NULL, IR_LPORT      }, /// 4
    { "--acp-port-remote",     _required_argument_, NULL, IR_RPORT      }, /// 5
    { "--acp-host-remote",     _required_argument_, NULL, IR_RHOST      }, /// 6
    ///
    { "--acp-taskid",          _required_argument_, NULL, IR_TASKID     }, /// 7
    { "--acp-size-smem",       _required_argument_, NULL, IR_SZSMEM_BL  }, /// 8
    { "--acp-size-smem-cl",    _required_argument_, NULL, IR_SZSMEM_CL  }, /// 9
    { "--acp-size-smem-dl",    _required_argument_, NULL, IR_SZSMEM_DL  }, /// 10
    { "--acp-ethernet-speed",  _required_argument_, NULL, IR_ETHSPEED   }, /// 11
    { 0,                       0,                   0,    0             }
} ;

#endif ///__INSIDE_ACPBL_INPUT_C__

///#endif //__INCLUDE_DEFAULT_INPUT__
