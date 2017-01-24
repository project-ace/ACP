#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>

void print_int_multiset(acp_multiset_t set)
{
    printf("    multiset[%016llx] num_ranks = %d, num_slots = %d\n", set.ga, set.num_ranks, set.num_slots);
    
    if (acp_procs() == 1) {
        acp_ga_t* directory = (uint64_t*)acp_query_address(set.ga);
        int rank;
        
        for (rank = 0; rank < set.num_ranks; rank++) {
            printf("      multiset[%016llx][%d] = %016llx\n", set.ga, rank, directory[rank]);
            int slot;
            
            for (slot = 0; slot < set.num_slots; slot++) {
                acp_ga_t list = directory[rank] + slot * 32 + 8;
                uint64_t* ptr = (uint64_t*)acp_query_address(list);
                acp_ga_t head = *ptr;
                acp_ga_t tail = *(ptr + 1);
                uint64_t num  = *(ptr + 2);
                acp_ga_t this = head;
                int elem_id = 0;
                
                printf("        multiset[%016llx][%d][%d] list = %016llx, head = %016llx, tail= %016llx, num = %llu\n", set.ga, rank, slot, list, head, tail, num);
                while (this != list && this != ACP_GA_NULL) {
                    ptr = (uint64_t*)acp_query_address(this);
                    acp_ga_t next  = *ptr;
                    acp_ga_t prev  = *(ptr + 1);
                    uint64_t count = *(ptr + 2);
                    uint64_t size  = *(ptr + 3);
                    uint8_t* v = (uint8_t*)(ptr + 4);
                    int i;
                    printf("            elem[%d][%016llx] next = %016llx, prev = %016llx, count = %llx, key = '", elem_id, this, next, prev, count);
                    for (i = 0; i < size; i++) printf("%c", *v++);
                    printf("'\n");
                    this = next;
                    elem_id++;
                }
            }
        }
    } else {
        acp_multiset_it_t it;
        for (it = acp_begin_multiset(set); it.rank < it.set.num_ranks; it = acp_increment_multiset_it(it)) {
            printf("        rank = %d, slot = %d, elem = %016llx: ", it.rank, it.slot, it.elem);
            acp_element_t key = acp_dereference_multiset_it(it);
            printf("key.size = %d\n", key.size);
            ;
        }
    }
    
    return;
}

int main(int argc, char** argv)
{
    setbuf(stdout, NULL);
    acp_init(&argc, &argv);
    
    int rank = acp_rank();
    
    if (rank == 0) {
        acp_ga_t buf = acp_malloc(512, rank);
        if (buf == ACP_GA_NULL) return 1;
        
        acp_ga_t buf_key = buf;
        void* ptr = acp_query_address(buf);
        volatile uint8_t* tmp_key   = (volatile uint8_t*)ptr;
        acp_multiset_t set, tmpset;
        acp_multiset_it_t it;
        int i, j;
        acp_element_t key;
        key.ga = buf_key;
        key.size = 0;
        
        int ranks[] = { 0 };
        
        printf("** create blank multiset\n");
        set = acp_create_multiset(1, ranks, 4 ,rank);
        print_int_multiset(set);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_multiset(set));
        
        printf("** insert 7 keys\n");
        for (i = 0; i < 16; i++) {
            for (j = 0; j <= i; j++) tmp_key[j] = 'A' + j;
            key.size = i + 1;
            acp_insert_multiset(set, key);
        }
        print_int_multiset(set);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_multiset(set));
        
        printf("** dereference first 10 keys\n");
        it = acp_begin_multiset(set);
        for (i = 0; i < 10; i++) {
            printf("    it.rank = %d, it.slot = %d, it.elem = %016llx\n", it.rank, it.slot, it.elem);
            acp_element_t key = acp_dereference_multiset_it(it);
            printf("      elem[%d] key.ga = %016llx, key.size = %d\n", i, key.ga, key.size);
            it = acp_increment_multiset_it(it);
        }
        
        printf("** create another multiset\n");
        tmpset = acp_create_multiset(1, ranks, 4, rank);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** assign\n");
        acp_assign_multiset(tmpset, set);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** remove 'ABCDEF'\n");
        key.size = 6;
        for (j = 0; j < key.size; j++) tmp_key[j] = 'A' + j;
        acp_remove_multiset(set, key);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** find\n");
        for (i = 4; i < 8; i++) {
            printf("    key = '");
            for (j = 0; j <= i; j++) {
                uint8_t c = 'A' + j;
                tmp_key[j] = c;
                printf("%c", c);
            }
            printf("'\n");
            key.size = i + 1;
            it = acp_find_multiset(set, key);
            printf("        it.rank = %d, it.slot = %d, it.elem = %016llx\n", it.rank, it.slot, it.elem);
        }
        
        printf("** merge\n");
        acp_merge_multiset(set, tmpset);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** retrieve\n");
        for (i = 4; i < 8; i++) {
            printf("    key = '");
            for (j = 0; j <= i; j++) {
                uint8_t c = 'A' + j;
                tmp_key[j] = c;
                printf("%c", c);
            }
            printf("'\n");
            key.size = i + 1;
            uint64_t count = acp_retrieve_multiset(set, key);
            printf("        count = %llx\n", count);
        }
        
        printf("** clear\n");
        acp_clear_multiset(tmpset);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** move\n");
        acp_move_multiset(tmpset, set);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_multiset(set));
        
        printf("** swap\n");
        acp_swap_multiset(set, tmpset);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** remove 'ABCD'\n");
        key.size = 4;
        for (j = 0; j < key.size; j++) tmp_key[j] = 'A' + j;
        acp_remove_multiset(set, key);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** remove 'ABCDEF'\n");
        key.size = 6;
        for (j = 0; j < key.size; j++) tmp_key[j] = 'A' + j;
        acp_remove_multiset(set, key);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** remove all 'ABC'\n");
        key.size = 3;
        for (j = 0; j < key.size; j++) tmp_key[j] = 'A' + j;
        acp_remove_all_multiset(set, key);
        print_int_multiset(set);
        print_int_multiset(tmpset);
        
        printf("** destroy\n");
        acp_destroy_multiset(tmpset);
        acp_destroy_multiset(set);
    }
    
    acp_finalize();
    return 0;
}

