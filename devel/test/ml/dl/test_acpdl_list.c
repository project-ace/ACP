#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>

void print_int_list(acp_list_t list)
{
    uint64_t* ptr = (uint64_t*)acp_query_address(list.ga);
    acp_ga_t head = *ptr;
    uint64_t tail = *(ptr + 1);
    uint64_t num  = *(ptr + 2);
    acp_ga_t this = head;
    int i = 0;
    
    printf("  list[%016llx] head = %016llx, tail= %016llx, num = %llu\n", list.ga, head, tail, num);
    while (this != list.ga && this != ACP_GA_NULL) {
        ptr = (uint64_t*)acp_query_address(this);
        acp_ga_t next = *ptr;
        acp_ga_t prev = *(ptr + 1);
        uint64_t size = *(ptr + 2);
        int v = *(int*)(ptr + 3);
        printf("    elem[%d][%016llx] next = %016llx, prev = %016llx, size = %llu, value = %d\n", i, this, next, prev, size, v);
        this = next;
    }
    printf("\n");
    return;
}

int main(int argc, char** argv)
{
    setbuf(stdout, NULL);
    acp_init(&argc, &argv);
    
    int rank = acp_rank();
    
    if (rank == 0) {
        acp_ga_t buf = acp_malloc(sizeof(int), rank);
        if (buf == ACP_GA_NULL) return 1;
        void* ptr = acp_query_address(buf);
        volatile int* tmp = (volatile int*)ptr;
        acp_list_t d, e;
        acp_list_it_t it, start, end;
        int i;
        acp_element_t elem;
        elem.ga = buf;
        elem.size = sizeof(int);
        
        printf("** create blank list\n");
        d = acp_create_list(rank);
        print_int_list(d);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_list(d));
        
        printf("** push_back 7 times\n");
        for (i = 101; i <= 107; i++) {
            *tmp = i;
            acp_push_back_list(d, elem, rank);
            print_int_list(d);
        }
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_list(d));
        
        printf("** dereference 7 times\n   ");
        it = acp_begin_list(d);
        printf("  it.list.ga = %016llx, it.elem = %016llx\n", it.list.ga, it.elem);
        for (i = 0; i < 7; i++) {
            acp_element_t elemtmp = acp_dereference_list_it(it);
            printf(" %d,", *(int*)acp_query_address(elemtmp.ga));
            it = acp_advance_list_it(it, 1);
        }
        printf("\n");
        
        printf("** destroy\n");
        acp_destroy_list(d);
        
        printf("** create list\n");
        d = acp_create_list(rank);
        print_int_list(d);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_list(d));
        
        printf("** push_back\n");
        *tmp = 200;
        acp_push_back_list(d, elem, rank);
        print_int_list(d);
        
        printf("** clear\n");
        acp_clear_list(d);
        print_int_list(d);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_list(d));
        
        printf("** destroy\n");
        acp_destroy_list(d);
        
        printf("** create list\n");
        d = acp_create_list(rank);
        print_int_list(d);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_list(d));
        
        printf("** insert 5 times from begin iterator\n");
        it = acp_begin_list(d);
        printf("  it.list.ga = %016llx, it.elem = %016llx\n", it.list.ga, it.elem);
        for (i = 301; i <= 305; i++) {
            *tmp = i;
            it = acp_insert_list(it, elem, rank);
            print_int_list(d);
        }
        
        printf("** insert 2 times from begin iterator + 2\n");
        it = acp_begin_list(d);
        it = acp_advance_list_it(it, 2);
        for (i = 401; i <= 402; i++) {
            *tmp = i;
            it = acp_insert_list(it, elem, rank);
            print_int_list(d);
        }
        
        printf("** create another list\n");
        e = acp_create_list(rank);
        print_int_list(d);
        print_int_list(e);
        
        printf("** assign\n");
        acp_assign_list(e, d);
        print_int_list(d);
        print_int_list(e);
        
        printf("** pop_back\n");
        acp_pop_back_list(d);
        print_int_list(d);
        print_int_list(e);
        
        printf("** erase @ begin iterator + 1\n");
        it = acp_begin_list(d);
        it = acp_advance_list_it(it, 1);
        acp_erase_list(it);
        print_int_list(d);
        print_int_list(e);
        
        printf("** swap\n");
        acp_swap_list(d, e);
        print_int_list(d);
        print_int_list(e);
        
        printf("** assign range from 1 to 3\n");
        start = acp_begin_list(d);
        start = acp_advance_list_it(start, 1);
        end = acp_begin_list(d);
        end = acp_advance_list_it(end, 3);
        acp_assign_range_list(e, start, end);
        print_int_list(d);
        print_int_list(e);
        
        printf("** insert range from 3 to 5 at 1\n");
        start = acp_begin_list(d);
        start = acp_advance_list_it(start, 3);
        end = acp_begin_list(d);
        end = acp_advance_list_it(end, 5);
        it = acp_begin_list(e);
        it = acp_advance_list_it(it, 1);
        acp_insert_range_list(it, start, end);
        print_int_list(d);
        print_int_list(e);
        
        printf("** erase 3 times @ begin iterator\n");
        it = acp_begin_list(d);
        it = acp_erase_list(it);
        it = acp_erase_list(it);
        acp_erase_list(it);
        print_int_list(d);
        
        printf("** push_front 3 times\n");
        for (i = 501; i <= 503; i++) {
            *tmp = i;
            acp_push_front_list(d, elem, rank);
            print_int_list(d);
        }
        
        printf("** pop_front 4 times\n");
        for (i = 1; i <= 4; i++) {
            acp_pop_front_list(d);
            print_int_list(d);
        }
    }
    
    acp_finalize();
    return 0;
}

