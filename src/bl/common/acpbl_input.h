#ifndef __INCLUDE_ACPBL_INPUT__
#define __INCLUDE_ACPBL_INPUT__

typedef struct {
    uint64_t value;
} iacpbl_option_nil_t;

typedef struct {
    uint64_t value;
    uint64_t min;
    uint64_t max;
} iacpbl_option_uint_t;

typedef struct {
    double value;
    double min;
    double max;
} iacpbl_option_double_t;

typedef struct {
    char string[BUFSIZ];
} iacpbl_option_string_t;

typedef struct {
    iacpbl_option_string_t portfile;
    iacpbl_option_uint_t offsetrank;
    iacpbl_option_uint_t myrank;
    iacpbl_option_uint_t nprocs;
    iacpbl_option_uint_t lport;
    iacpbl_option_string_t rhost;
    uint32_t rhost_ip;
    iacpbl_option_uint_t rport;
    iacpbl_option_uint_t taskid;
    iacpbl_option_uint_t szsmem;
    iacpbl_option_uint_t szsmemcl;
    iacpbl_option_uint_t szsmemdl;
    iacpbl_option_uint_t mhooksmall;
    iacpbl_option_uint_t mhooklarge;
    iacpbl_option_uint_t mhookhuge;
    iacpbl_option_uint_t mhooklow;
    iacpbl_option_uint_t mhookhigh;
    iacpbl_option_uint_t ethspeed;
} iacpbl_option_t;

extern iacpbl_option_t iacpbl_option;
extern int iacpbl_interpret_option( int *argc, char ***argv ) ;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
