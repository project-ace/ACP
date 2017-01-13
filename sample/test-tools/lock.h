//#include "procinfo.h"

struct lock{
    int *lockbody;
    acp_atkey_t atkey;
    procinfo_t info;
};

typedef struct lock lock_t;

extern lock_t lock_create();
extern int lock_acquire(lock_t lock, int rank);
extern int lock_release(lock_t lock, int rank);
extern int lock_free(lock_t lock);


