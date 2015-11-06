#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>
#include "acpbl_sync.h"

/*
Xeon 5160     2933.333 (hana)
Xeon E5520    2266.667 (rx)
Opteron 8354  2200.000 (hx)
*/
#define MHZ 2266.667

#define REP 1024

int byte_table[] = {    8,   16,   32,   64,  128,  256,  512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576, 0};
int rep_table[]  = { 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,  1024,  1024,   512,    256,    128,     64,      32, 0};

unsigned char dummy[32768];

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
    acp_map_t m;
    int ranks[2];
    acp_atkey_t dummy_atkey, buf_atkey;
    acp_ga_t dummy_ga, buf_ga;
    
    setbuf(stdout, NULL);
    
    acp_init(&argc, &argv);
    
    rank = acp_rank();
    
    if (rank == 0) {
        printf("# acp_malloc() local\n              #repetitions      t[usec]\n");
        b = 22259;
        r = REP;
        t0 = get_clock();
        for (j = 0; j < r; j++) {
            gmem[j] = acp_malloc(b, 0);
            b = (b * 2039 + 9377) & 0x7fff;
            b += b ? 0 : 0x8000;
        }
        t1 = get_clock();
        printf("%26d%13.3f\n", r, (t1 - t0)/MHZ/r);
        
        printf("# acp_free() local\n              #repetitions      t[usec]\n");
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
        
        printf("# acp_malloc() remote\n              #repetitions      t[usec]\n");
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
        
        printf("# acp_free() remote\n              #repetitions      t[usec]\n");
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
        
        b = 22259;
        for (j = 0; j < 32768; j++) {
            dummy[j] = (b >> 7) & 0xff;
            b = (b * 2039 + 9377) & 0x7fff;
            b += b ? 0 : 0x8000;
        }
        
        dummy_atkey = acp_register_memory((void*)dummy, sizeof(dummy), 0);
        dummy_ga = acp_query_ga(dummy_atkey, (void*)dummy);
        
        printf("# acp_insert_map() local 1 process\n              #repetitions      t[usec]\n");
        r = REP;
        ranks[0] = 0;
        m = acp_create_map(1, (void*)ranks, 128, 0);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_insert_map(m, dummy_ga + j, 32, dummy_ga + j, 32);
        t1 = get_clock();
        printf("%26d%13.3f\n", r, (t1 - t0)/MHZ/r);
        
        printf("# acp_find_map()\n              #repetitions      t[usec]\n");
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_find_map(m, dummy_ga + *(unsigned int*)(dummy + j) % (REP*2), 32);
        t1 = get_clock();
        printf("%26d%13.3f\n", r, (t1 - t0)/MHZ/r);
        acp_destroy_map(m);
        
        printf("# acp_insert_map() remote 1 process\n              #repetitions      t[usec]\n");
        r = REP;
        ranks[0] = 1;
        m = acp_create_map(1, (void*)ranks, 128, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_insert_map(m, dummy_ga + j, 32, dummy_ga + j, 32);
        t1 = get_clock();
        printf("%26d%13.3f\n", r, (t1 - t0)/MHZ/r);
        
        printf("# acp_find_map()\n              #repetitions      t[usec]\n");
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_find_map(m, dummy_ga + *(unsigned int*)(dummy + j) % (REP*2), 32);
        t1 = get_clock();
        printf("%26d%13.3f\n", r, (t1 - t0)/MHZ/r);
        acp_destroy_map(m);
        
        printf("# acp_insert_map() remote 4 process\n              #repetitions      t[usec]\n");
        r = REP;
        ranks[0] = 0;
        ranks[1] = 1;
        ranks[2] = 2;
        ranks[3] = 3;
        m = acp_create_map(4, (void*)ranks, 32, 1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_insert_map(m, dummy_ga + j, 32, dummy_ga + j, 32);
        t1 = get_clock();
        printf("%26d%13.3f\n", r, (t1 - t0)/MHZ/r);
        
        printf("# acp_find_map()\n              #repetitions      t[usec]\n");
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_find_map(m, dummy_ga + *(unsigned int*)(dummy + j) % (REP*2), 32);
        t1 = get_clock();
        printf("%26d%13.3f\n", r, (t1 - t0)/MHZ/r);
        acp_destroy_map(m);
        
        buf_atkey = acp_register_memory((void*)buf, sizeof(buf), 0);
        buf_ga = acp_query_ga(buf_atkey, (void*)buf);
        
        printf("# acp_push_back_list() local\n       #bytes #repetitions      t[usec]\n");
        b = 32;
        r = REP;
        l = acp_create_list(0);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_push_back_list(l, buf_ga, b, 0);
        t1 = get_clock();
        printf("%13d%13d%13.3f\n", b, r, (t1 - t0)/MHZ/r);
        acp_destroy_list(l);
        
        printf("# acp_push_back_list() remote\n       #bytes #repetitions      t[usec]\n");
        b = 32;
        r = REP;
        l = acp_create_list(1);
        t0 = get_clock();
        for (j = 0; j < r; j++)
            acp_push_back_list(l, buf_ga, b, 1);
        t1 = get_clock();
        printf("%13d%13d%13.3f\n", b, r, (t1 - t0)/MHZ/r);
        acp_destroy_list(l);
        
        printf("# acp_swap_vector() local and local\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        for ( i = 0; byte_table[i] > 0; i++) {
            b = byte_table[i];
            r = rep_table[i];
            v = acp_create_vector(b, 0);
            w = acp_create_vector(b, 0);
            t0 = get_clock();
            for (j = 0; j < r; j++)
                acp_swap_vector(v, w);
            t1 = get_clock();
            printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
            acp_destroy_vector(v);
            acp_destroy_vector(w);
        }
        
        printf("# acp_swap_vector() local and remote\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        for ( i = 0; byte_table[i] > 0; i++) {
            b = byte_table[i];
            r = rep_table[i];
            v = acp_create_vector(b, 0);
            w = acp_create_vector(b, 1);
            t0 = get_clock();
            for (j = 0; j < r; j++)
                acp_swap_vector(v, w);
            t1 = get_clock();
            printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
            acp_destroy_vector(v);
            acp_destroy_vector(w);
        }
        
        printf("# acp_swap_vector() remote and remote\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
        for ( i = 0; byte_table[i] > 0; i++) {
            b = byte_table[i];
            r = rep_table[i];
            v = acp_create_vector(b, 1);
            w = acp_create_vector(b, 2);
            t0 = get_clock();
            for (j = 0; j < r; j++)
                acp_swap_vector(v, w);
            t1 = get_clock();
            printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
            acp_destroy_vector(v);
            acp_destroy_vector(w);
        }
    }
    
    acp_finalize();
    
    return 0;
}

