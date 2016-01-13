#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>

void print_int_deque(acp_deque_t deque)
{
    uint64_t* ptr = (uint64_t*)acp_query_address(deque.ga);
    acp_ga_t ga     = *ptr;
    uint64_t offset = *(ptr + 1);
    uint64_t size   = *(ptr + 2);
    uint64_t max    = *(ptr + 3);
    int i;
    
    printf("  deque[%016llx] ptr = %016llx, offset = %llu, size = %llu, capacity = %llu\n", deque.ga, ga, offset, size, max);
    if (size > 0) {
        printf("   ");
        for (i = 0; i < size; i += sizeof(int)) printf(" %d,", *(int*)acp_query_address(ga + ((offset + i) % max)));
        printf("\n");
    }
    return;
}

int main(int argc, char** argv)
{
    acp_init(&argc, &argv);
    
    int rank = acp_rank();
    
    if (rank == 0) {
        acp_ga_t buf = acp_malloc(sizeof(int), rank);
        if (buf == ACP_GA_NULL) return 1;
        void* ptr = acp_query_address(buf);
        volatile int* tmp = (volatile int*)ptr;
        acp_deque_t d, e;
        acp_deque_it_t it, start, end;
        int i;
        
        printf("** create blank deque\n");
        d = acp_create_deque(0, rank);
        print_int_deque(d);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_deque(d));
        
        printf("** push_back 7 times\n");
        for (i = 101; i <= 107; i++) {
            *tmp = i;
            acp_push_back_deque(d, buf, sizeof(int));
            print_int_deque(d);
        }
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_deque(d));
        
        printf("** at 7 times\n   ");
        for (i = 0; i < 7; i++) {
            acp_ga_t ga = acp_at_deque(d, sizeof(int) * i);
            printf(" %d,", *(int*)acp_query_address(ga));
        }
        printf("\n");
        
        printf("** dereference 7 times\n   ");
        it = acp_begin_deque(d);
        for (i = 0; i < 7; i++) {
            acp_pair_t pair = acp_dereference_deque_it(it, sizeof(int));
            printf(" %d,", *(int*)acp_query_address(pair.first.ga));
            it = acp_advance_deque_it(it, sizeof(int));
        }
        printf("\n");
        
        printf("** destroy\n");
        acp_destroy_deque(d);
        
        printf("** create sizeof(int)*6 deque\n");
        d = acp_create_deque(sizeof(int)*6, rank);
        print_int_deque(d);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_deque(d));
        
        printf("** push_back\n");
        *tmp = 200;
        acp_push_back_deque(d, buf, sizeof(int));
        print_int_deque(d);
        
        printf("** clear\n");
        acp_clear_deque(d);
        print_int_deque(d);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_deque(d));
        
        printf("** destroy\n");
        acp_destroy_deque(d);
        
        printf("** create blank deque\n");
        d = acp_create_deque(0, rank);
        print_int_deque(d);
        
        printf("** reserve sizeof(int)*3 deque\n");
        acp_reserve_deque(d, sizeof(int)*3);
        print_int_deque(d);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_deque(d));
        
        printf("** insert 5 times from begin iterator\n");
        it = acp_begin_deque(d);
        for (i = 301; i <= 305; i++) {
            *tmp = i;
            it = acp_insert_deque(it, buf, sizeof(int));
            print_int_deque(d);
        }
        
        printf("** insert 2 times from begin iterator + sizeof(int)*2\n");
        it = acp_begin_deque(d);
        it = acp_advance_deque_it(it, sizeof(int)*2);
        for (i = 401; i <= 402; i++) {
            *tmp = i;
            it = acp_insert_deque(it, buf, sizeof(int));
            print_int_deque(d);
        }
        
        printf("** create another deque\n");
        e = acp_create_deque(0, rank);
        print_int_deque(d);
        print_int_deque(e);
        
        printf("** assign\n");
        acp_assign_deque(e, d);
        print_int_deque(d);
        print_int_deque(e);
        
        printf("** pop_back sizeof(int)\n");
        acp_pop_back_deque(d, sizeof(int));
        print_int_deque(d);
        print_int_deque(e);
        
        printf("** erase sizeof(int) @ begin iterator + sizeof(int)\n");
        it = acp_begin_deque(d);
        it = acp_advance_deque_it(it, sizeof(int));
        acp_erase_deque(it, sizeof(int));
        print_int_deque(d);
        print_int_deque(e);
        
        printf("** swap\n");
        acp_swap_deque(d, e);
        print_int_deque(d);
        print_int_deque(e);
        
        printf("** assign range from sizeof(int) to sizeof(int)*3\n");
        start = acp_begin_deque(d);
        start = acp_advance_deque_it(start, sizeof(int));
        end = acp_begin_deque(d);
        end = acp_advance_deque_it(end, sizeof(int)*3);
        acp_assign_range_deque(e, start, end);
        print_int_deque(d);
        print_int_deque(e);
        
        printf("** erase sizeof(int)*3 @ begin iterator\n");
        it = acp_begin_deque(d);
        acp_erase_deque(it, sizeof(int)*3);
        print_int_deque(d);
        
        printf("** push_front 3 times\n");
        for (i = 501; i <= 503; i++) {
            *tmp = i;
            acp_push_front_deque(d, buf, sizeof(int));
            print_int_deque(d);
        }
        
        printf("** pop_front 4 times\n");
        for (i = 1; i <= 4; i++) {
            acp_pop_front_deque(d, sizeof(int));
            print_int_deque(d);
        }
        
        printf("** push_front contents of deque e\n");
        acp_push_front_deque(d, *(uint64_t*)acp_query_address(e.ga), sizeof(int)*2);
        print_int_deque(d);
        print_int_deque(e);
        
        printf("** pop_front sizeof(int)*3\n");
        acp_pop_front_deque(d, sizeof(int)*3);
        print_int_deque(d);
    }
    
    acp_finalize();
    return 0;
}

