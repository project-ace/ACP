struct procinfo{
    int size;
    int sb;
    int nb;
    acp_atkey_t bodykey;
    acp_ga_t bodyga;
    acp_ga_t *body;
    void *cachebody;
    acp_ga_t cachega;
    int *cacheblockid;
    int *cachectr;
    int cachesz;
    // group info will be added
};

typedef struct procinfo procinfo_t;

procinfo_t procinfo_create(void *data, int size);
int procinfo_queryinfo(procinfo_t pi, int rank, void *data);
int procinfo_querysize(procinfo_t pi);
// int procinfo_resetinfo(procinfo_t pi, void *data);
int procinfo_free(procinfo_t pi);
