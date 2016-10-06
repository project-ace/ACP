#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>

void print_int_map(acp_map_t map)
{
    printf("    map[%016llx] num_ranks = %d, num_slots = %d\n", map.ga, map.num_ranks, map.num_slots);
    
    if (acp_procs() == 1) {
        acp_ga_t* directory = (uint64_t*)acp_query_address(map.ga);
        int rank;
        
        for (rank = 0; rank < map.num_ranks; rank++) {
            printf("      map[%016llx][%d] = %016llx\n", map.ga, rank, directory[rank]);
            int slot;
            
            for (slot = 0; slot < map.num_slots; slot++) {
                acp_ga_t list = directory[rank] + slot * 32 + 8;
                uint64_t* ptr = (uint64_t*)acp_query_address(list);
                acp_ga_t head = *ptr;
                acp_ga_t tail = *(ptr + 1);
                uint64_t num  = *(ptr + 2);
                acp_ga_t this = head;
                int elem_id = 0;
                
                printf("        map[%016llx][%d][%d] list = %016llx, head = %016llx, tail= %016llx, num = %llu\n", map.ga, rank, slot, list, head, tail, num);
                while (this != list && this != ACP_GA_NULL) {
                    ptr = (uint64_t*)acp_query_address(this);
                    acp_ga_t next = *ptr;
                    acp_ga_t prev = *(ptr + 1);
                    uint64_t size = *(ptr + 2);
                    uint64_t key_size = *(ptr + 3);
                    uint64_t value_size = size - 8 - key_size;
                    uint8_t* v = (uint8_t*)(ptr + 4);
                    int i;
                    printf("            elem[%d][%016llx] next = %016llx, prev = %016llx, key = '", elem_id, this, next, prev);
                    for (i = 0; i < key_size; i++) printf("%c", *v++);
                    printf("', value = '");
                    for (i = 0; i < value_size; i++) printf("%c", *v++);
                    printf("'\n");
                    this = next;
                    elem_id++;
                }
            }
        }
    } else {
        acp_map_it_t it;
        for (it = acp_begin_map(map); it.rank < it.map.num_ranks; it = acp_increment_map_it(it)) {
            printf("        rank = %d, slot = %d, elem = %016llx: ", it.rank, it.slot, it.elem);
            acp_pair_t pairtmp = acp_dereference_map_it(it);
            printf("key.size = %d, value.size = %d\n", pairtmp.first.size, pairtmp.second.size);
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
        acp_ga_t buf_value = buf + 256;
        void* ptr = acp_query_address(buf);
        volatile uint8_t* tmp_key   = (volatile uint8_t*)ptr;
        volatile uint8_t* tmp_value = (volatile uint8_t*)(ptr + 256);
        acp_map_t map, tmpmap;
        acp_map_it_t it;
        int i, j;
        acp_pair_t pair;
        pair.first.ga = buf_key;
        pair.first.size = 0;
        pair.second.ga = buf_value;
        pair.second.size = 0;
        
        int ranks[] = { 0 };
        
        printf("** create blank map\n");
        map = acp_create_map(1, ranks, 4 ,rank);
        print_int_map(map);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_map(map));
        
        printf("** insert 7 pairs\n");
        for (i = 0; i < 16; i++) {
            for (j = 0; j <= i; j++) tmp_key[j] = 'A' + j;
            pair.first.size = i + 1;
            for (j = 0; j < 15; j++) tmp_value[j] = 'a' + i;
            pair.second.size = 15;
            acp_insert_map(map, pair);
        }
        print_int_map(map);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_map(map));
        
        printf("** dereference first 10 pairs\n");
        it = acp_begin_map(map);
        for (i = 0; i < 10; i++) {
            printf("    it.rank = %d, it.slot = %d, it.elem = %016llx\n", it.rank, it.slot, it.elem);
            acp_pair_t pairtmp = acp_dereference_map_it(it);
            printf("      elem[%d] key.ga = %016llx, key.size = %d, value.ga = %016llx, value.size = %d\n", i, pairtmp.first.ga, pairtmp.first.size, pairtmp.second.ga, pairtmp.second.size);
            it = acp_increment_map_it(it);
        }
        
        printf("** create another map\n");
        tmpmap = acp_create_map(1, ranks, 4, rank);
        print_int_map(map);
        print_int_map(tmpmap);
        
        printf("** assign\n");
        acp_assign_map(tmpmap, map);
        print_int_map(map);
        print_int_map(tmpmap);
        
        printf("** remove 'ABCDEF'\n");
        for (j = 0; j < 6; j++) tmp_key[j] = 'A' + j;
        pair.first.size = 6;
        acp_remove_map(map, pair.first);
        print_int_map(map);
        print_int_map(tmpmap);
        
        printf("** find\n");
        for (i = 4; i < 8; i++) {
            printf("    key = '");
            for (j = 0; j <= i; j++) {
                uint8_t c = 'A' + j;
                tmp_key[j] = c;
                printf("%c", c);
            }
            printf("'\n");
            pair.first.size = i + 1;
            it = acp_find_map(map, pair.first);
            printf("        it.rank = %d, it.slot = %d, it.elem = %016llx\n", it.rank, it.slot, it.elem);
        }
        
        printf("** retreive\n");
        for (i = 4; i < 8; i++) {
            printf("    key = '");
            for (j = 0; j <= i; j++) {
                uint8_t c = 'A' + j;
                tmp_key[j] = c;
                printf("%c", c);
            }
            pair.first.size = i + 1;
            pair.second.size = 256;
            size_t ret = acp_retrieve_map(map, pair);
            printf("' => retrun %d\n", ret);
        }
        
        printf("** clear\n");
        acp_clear_map(tmpmap);
        print_int_map(map);
        print_int_map(tmpmap);
        
        printf("** move\n");
        acp_move_map(tmpmap, map);
        print_int_map(map);
        print_int_map(tmpmap);
        
        printf("** empty\n");
        printf("    result = %d\n", acp_empty_map(map));
        
        printf("** swap\n");
        acp_swap_map(map, tmpmap);
        print_int_map(map);
        print_int_map(tmpmap);
        
        printf("** destroy\n");
        acp_destroy_map(tmpmap);
        acp_destroy_map(map);
    }
    
    acp_finalize();
    return 0;
}

