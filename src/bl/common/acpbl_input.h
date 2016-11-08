#ifndef __INCLUDE_ACPBL_INPUT__
#define __INCLUDE_ACPBL_INPUT__

#define IR_MULTIRUN         0
#define IR_MULTIRUN_OFFSET  1
#define IR_MYRANK           2
#define IR_NPROCS           3
#define IR_LPORT            4
#define IR_RPORT            5
#define IR_RHOST            6
#define IR_TASKID           7
#define IR_SZSMEM_BL        8
#define IR_SZSMEM_CL        9
#define IR_SZSMEM_DL       10
#define _NIR_              11

typedef struct {
    int         flg_set [ _NIR_ ] ;
    uint64_t    n_inputs[ _NIR_ ] ;
    char       *s_inputs[ _NIR_ ] ;
} acpbl_input_t ;

extern int iacp_connection_information( int *argc, char ***argv, acpbl_input_t *ait ) ;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
