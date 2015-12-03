#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>

void print_int_vector(acp_vector_t vector)
{
    uint64_t* ptr = (uint64_t*)acp_query_address(vector.ga);
    acp_ga_t ga = (acp_ga_t)*ptr;
    uint64_t size = (uint64_t)*(ptr + 1);
    uint64_t max  = (uint64_t)*(ptr + 2);
    int i;
    
    printf("  vector[%016llx] ptr = %016llx, size = %llu, capacity = %llu\n", vector.ga, ga, size, max);
    if (size > 0) {
        printf("   ");
        for (i = 0; i < size; i += sizeof(int)) printf(" %d,", *(int*)acp_query_address(ga + i));
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
        acp_vector_t v, w;
        acp_vector_it_t it, start, end;
        int i;
        
        printf("** create blank vector\n");
        v = acp_create_vector(0, rank);
        print_int_vector(v);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_vector(v));
        
        printf("** push_back 7 times\n");
        for (i = 101; i <= 107; i++) {
            *tmp = i;
            acp_push_back_vector(v, buf, sizeof(int));
            print_int_vector(v);
        }
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_vector(v));
        
        printf("** at 7 times\n   ");
        for (i = 0; i < 7; i++) {
            acp_ga_t ga = acp_at_vector(v, sizeof(int) * i);
            printf(" %d,", *(int*)acp_query_address(ga));
        }
        printf("\n");
        
        printf("** dereference 7 times\n   ");
        it = acp_begin_vector(v);
        for (i = 0; i < 7; i++) {
            acp_ga_t ga = acp_dereference_vector_it(it);
            printf(" %d,", *(int*)acp_query_address(ga));
            it = acp_advance_vector_it(it, sizeof(int));
        }
        printf("\n");
        
        printf("** destroy\n");
        acp_destroy_vector(v);
        
        printf("** create sizeof(int)*6 vector\n");
        v = acp_create_vector(sizeof(int)*6, rank);
        print_int_vector(v);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_vector(v));
        
        printf("** push_back\n");
        *tmp = 200;
        acp_push_back_vector(v, buf, sizeof(int));
        print_int_vector(v);
        
        printf("** clear\n");
        acp_clear_vector(v);
        print_int_vector(v);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_vector(v));
        
        printf("** destroy\n");
        acp_destroy_vector(v);
        
        printf("** create blank vector\n");
        v = acp_create_vector(0, rank);
        print_int_vector(v);
        
        printf("** reserve sizeof(int)*3 vector\n");
        acp_reserve_vector(v, sizeof(int)*3);
        print_int_vector(v);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_vector(v));
        
        printf("** insert 5 times from begin iterator\n");
        it = acp_begin_vector(v);
        for (i = 301; i <= 305; i++) {
            *tmp = i;
            it = acp_insert_vector(it, buf, sizeof(int));
            print_int_vector(v);
        }
        
        printf("** insert 2 times from begin iterator + sizeof(int)*2\n");
        it = acp_begin_vector(v);
        it = acp_advance_vector_it(it, sizeof(int)*2);
        for (i = 401; i <= 402; i++) {
            *tmp = i;
            it = acp_insert_vector(it, buf, sizeof(int));
            print_int_vector(v);
        }
        
        printf("** create another vector\n");
        w = acp_create_vector(0, rank);
        print_int_vector(v);
        print_int_vector(w);
        
        printf("** assign\n");
        acp_assign_vector(w, v);
        print_int_vector(v);
        print_int_vector(w);
        
        printf("** pop_back sizeof(int)\n");
        acp_pop_back_vector(v, sizeof(int));
        print_int_vector(v);
        print_int_vector(w);
        
        printf("** erase sizeof(int) @ begin iterator + sizeof(int)\n");
        it = acp_begin_vector(v);
        it = acp_advance_vector_it(it, sizeof(int));
        acp_erase_vector(it, sizeof(int));
        print_int_vector(v);
        print_int_vector(w);
        
        printf("** swap\n");
        acp_swap_vector(v, w);
        print_int_vector(v);
        print_int_vector(w);
        
        printf("** assign range from sizeof(int) to sizeof(int)*3\n");
        start = acp_begin_vector(v);
        start = acp_advance_vector_it(start, sizeof(int));
        end = acp_begin_vector(v);
        end = acp_advance_vector_it(end, sizeof(int)*3);
        acp_assign_range_vector(w, start, end);
        print_int_vector(v);
        print_int_vector(w);
    }
    
    acp_finalize();
    return 0;
}

