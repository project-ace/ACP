#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>
#include "acpbl_sync.h"

//#define MHZ 2933.333 //hana Xeon 5160
#define MHZ 2266.667 //rx200 Xeon E5520
//#define MHZ 2200.000 //hx Opteron 8354

#define REP 100

int main(int argc, char** argv)
{
    int rank;
    uint64_t t0, t1;
    int i, j, s, b, r;
    uint64_t gmem[REP];
    acp_vector_t v, w;
    acp_vector_t dupvec[REP];
    uint64_t buf[REP];
    acp_list_t l;
    
    setbuf(stdout, NULL);
    
    acp_init(&argc, &argv);
    
    rank = acp_rank();
    
    if (rank == 0) {
        /* local */
        printf("# Local acp_malloc()\n       #bytes #repetitions      t[usec]\n");
        s = 0;
        b = 22259;
        r = REP;
        t0 = get_clock();
        for (j = 0; j < r; j++) {
            gmem[j] = acp_malloc(b, 0);
            s += b;
            b = (b * 2039 + 9377) & 0x7fff;
            b += b ? 0 : 0x8000;
        }
        t1 = get_clock();
        printf("%13.1f%13d%13.3f\n", (double)s / r, r, (t1 - t0)/MHZ/r);
        
        printf("# Local acp_free()\n       #bytes #repetitions      t[usec]\n");
        b = 22259;
        r = REP;
        t0 = get_clock();
        for (j = 0; j < r; ) {
            s = (int)(b / 32768.0 * REP);
            if (gmem[s] != ACP_GA_NULL) {
                acp_free(gmem[s]);
                gmem[s] = ACP_GA_NULL;
                j++;
            }
            b = (b * 2039 + 9377) & 0x7fff;
        }
        t1 = get_clock();
        printf("%13.1f%13d%13.3f\n", (double)s / r, r, (t1 - t0)/MHZ/r);
        
        printf("# List push back local\n       #bytes #repetitions      t[usec]\n");
        b = 32;
        r = REP;
        l = acp_create_list(b, 0);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_push_back_list(l, (void*)buf, 0);
        t1 = get_clock();
        printf("%13d%13d%13.3f\n", b, r, (t1 - t0)/MHZ/r);
        acp_destroy_list(l);
        
        printf("# Vector duplicate local to local\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        b = 1024;
        r = REP;
        v = acp_create_vector(b, 1, 0);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            dupvec[j] = acp_duplicate_vector(v, 0);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        for (j = 0; j < r; j++)
            acp_destroy_vector(dupvec[j]);
        
        b = 65536;
        r = REP;
        v = acp_create_vector(b, 1, 0);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            dupvec[j] = acp_duplicate_vector(v, 0);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        for (j = 0; j < r; j++)
            acp_destroy_vector(dupvec[j]);
        
        printf("# Vector swap local and local\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        b = 1024;
        r = REP;
        v = acp_create_vector(b, 1, 0);
        w = acp_create_vector(b, 1, 0);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_swap_vector(v, w);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        acp_destroy_vector(w);
        
        b = 65536;
        r = REP;
        v = acp_create_vector(b, 1, 0);
        w = acp_create_vector(b, 1, 0);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_swap_vector(v, w);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        acp_destroy_vector(w);
        
        /* remote to local */
        printf("# List push back remote to local\n       #bytes #repetitions      t[usec]\n");
        b = 32;
        r = REP;
        l = acp_create_list(b, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_push_back_list(l, (void*)buf, 0);
        t1 = get_clock();
        printf("%13d%13d%13.3f\n", b, r, (t1 - t0)/MHZ/r);
        acp_destroy_list(l);
        
        printf("# Vector duplicate remote to local\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        b = 1024;
        r = REP;
        v = acp_create_vector(b, 1, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            dupvec[j] = acp_duplicate_vector(v, 0);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        for (j = 0; j < r; j++)
            acp_destroy_vector(dupvec[j]);
        
        b = 65536;
        r = REP;
        v = acp_create_vector(b, 1, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            dupvec[j] = acp_duplicate_vector(v, 0);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        for (j = 0; j < r; j++)
            acp_destroy_vector(dupvec[j]);
        
        /* local to remote */
        printf("# List push back local to remote\n       #bytes #repetitions      t[usec]\n");
        b = 32;
        r = REP;
        l = acp_create_list(b, 0);
        t0 = get_clock();
        for (j = 0; j < r; j++) {
            acp_push_back_list(l, (void*)buf, 1);
        }
        t1 = get_clock();
        printf("%13d%13d%13.3f\n", b, r, (t1 - t0)/MHZ/r);
        acp_destroy_list(l);
        
        printf("# Vector duplicate local to remote\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        b = 1024;
        r = REP;
        v = acp_create_vector(b, 1, 0);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            dupvec[j] = acp_duplicate_vector(v, 1);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        for (j = 0; j < r; j++)
            acp_destroy_vector(dupvec[j]);
        
        b = 65536;
        r = REP;
        v = acp_create_vector(b, 1, 0);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            dupvec[j] = acp_duplicate_vector(v, 1);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        for (j = 0; j < r; j++)
            acp_destroy_vector(dupvec[j]);
        
        /* local and remote */
        printf("# Vector swap local and remote\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        b = 1024;
        r = REP;
        v = acp_create_vector(b, 1, 0);
        w = acp_create_vector(b, 1, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_swap_vector(v, w);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        acp_destroy_vector(w);
        
        b = 65536;
        r = REP;
        v = acp_create_vector(b, 1, 0);
        w = acp_create_vector(b, 1, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_swap_vector(v, w);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        acp_destroy_vector(w);
        
        /* remote */
        printf("# Remote acp_malloc()\n       #bytes #repetitions      t[usec]\n");
        b = 22259;
        r = REP;
        t0 = get_clock();
        for (j = 0; j < r; j++) {
            gmem[j] = acp_malloc(b, 1);
            b = (b * 2039 + 9377) & 0x7fff;
            b += b ? 0 : 0x8000;
        }
        t1 = get_clock();
        printf("%26d%13.3f\n", r, (t1 - t0)/MHZ/r);
        
        printf("# Remote acp_free()\n       #bytes #repetitions      t[usec]\n");
        b = 22259;
        r = REP;
        t0 = get_clock();
        for (j = 0; j < r; ) {
            s = (int)(b / 32768.0 * REP);
            if (gmem[s] != ACP_GA_NULL) {
                acp_free(gmem[s]);
                gmem[s] = ACP_GA_NULL;
                j++;
            }
            b = (b * 2039 + 9377) & 0x7fff;
        }
        t1 = get_clock();
        printf("%26d%13.3f\n", r, (t1 - t0)/MHZ/r);
        
        printf("# List push back remote to remote\n       #bytes #repetitions      t[usec]\n");
        b = 32;
        r = REP;
        l = acp_create_list(b, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_push_back_list(l, (void*)buf, 2);
        t1 = get_clock();
        printf("%13d%13d%13.3f\n", b, r, (t1 - t0)/MHZ/r);
        acp_destroy_list(l);
        
        printf("# Vector duplicate remote to remote\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        b = 1024;
        r = REP;
        v = acp_create_vector(b, 1, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            dupvec[j] = acp_duplicate_vector(v, 2);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        for (j = 0; j < r; j++)
            acp_destroy_vector(dupvec[j]);
        
        b = 65536;
        r = REP;
        v = acp_create_vector(b, 1, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            dupvec[j] = acp_duplicate_vector(v, 2);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        for (j = 0; j < r; j++)
            acp_destroy_vector(dupvec[j]);
        
        printf("# Vector swap remote and remote\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        b = 1024;
        r = REP;
        v = acp_create_vector(b, 1, 1);
        w = acp_create_vector(b, 1, 2);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_swap_vector(v, w);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        acp_destroy_vector(w);
        
        b = 65536;
        r = REP;
        v = acp_create_vector(b, 1, 1);
        w = acp_create_vector(b, 1, 2);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_swap_vector(v, w);
        t1 = get_clock();
        printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
        acp_destroy_vector(v);
        acp_destroy_vector(w);
    }
    
    acp_finalize();
    
    return 0;
}

