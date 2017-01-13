#include <acp.h>

extern int msg_setup();
extern int msg_push(int rank, int tag, void *addr, int size);
extern int msg_pop(int rank, int tag, void *addr, int size);
extern int msg_free();
