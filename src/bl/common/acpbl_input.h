#ifndef __INCLUDE_ACPBL_INPUT__
#define __INCLUDE_ACPBL_INPUT__

#define __INSIDE_ACPBL_INPUT_H__
#include "acpbl_default_input.h"    /// IR_* and _NIR_ are defined
#undef  __INSIDE_ACPBL_INPUT_H__

typedef struct {
    int         flg_set [ _NIR_ ] ;
    uint64_t    u_inputs[ _NIR_ ] ;
    double      d_inputs[ _NIR_ ] ;
    char       *s_inputs[ _NIR_ ] ;
    int         argc ;
    char      **argv ;
} acpbl_input_t ;

extern int iacp_connection_information( int *argc, char ***argv, acpbl_input_t *ait ) ;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
