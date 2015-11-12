#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include "acp.h"

#define SHIFT_IT_ELEM_GA 24

int get_today(void) {
	time_t     current;
	struct tm  *local;

	time(&current);
	local = localtime(&current);
	return local->tm_mday;
}

void* get_list_elem(acp_list_it_t it,
					void *p,
					size_t size,
					acp_atkey_t ak,
					acp_handle_t ah ){
	
	acp_ga_t srcga, dstga;

	ak = acp_register_memory(p, size, 0);
	dstga = acp_query_ga(ak, p);

	srcga = it.elem + SHIFT_IT_ELEM_GA; 
	ah = acp_copy ( dstga, srcga, size, ACP_HANDLE_ALL );
	acp_complete(ah);
	acp_unregister_memory(ak);

	return p;
}

int* find_node_int( const int n,
					 acp_list_t l,
					 int *p,
					 acp_atkey_t ak,
					 acp_handle_t ah ){

	acp_list_it_t it = acp_begin_list(l); 

	while( it.elem != acp_end_list(l).elem ) {
		if( it.elem == n ) {
			p = (int *)get_list_elem( it, p, sizeof(int), ak, ah );
			std::cout << "<< " << *p << ">> \t";
		}else{
			std::cout << *p << "\t";
		}
		it = acp_increment_list_it(it);
	}
	return p;
	std::cout << std::endl;
}

int insert_mark(acp_list_t l, const int n, const int mark, const int rank){
	std::cout << "-------- insert mark (" << mark << ") --------\n";

	acp_ga_t stga, ga;
	acp_ga_t* stla;
	acp_list_it_t it = acp_begin_list(l);
	while( it.elem != acp_end_list(l).elem ) {
		if( it.elem == n ) {
			stga = acp_query_starter_ga(acp_rank() );
			stla = (acp_ga_t *)acp_query_address(stga);
			*((int *)stla) = mark;
			acp_push_back_list(l, stga, sizeof(n), rank);
		}
		it = acp_increment_list_it(it); 
	}
	it = acp_begin_list(l);
	while( it.elem != acp_end_list(l).elem ) {
		std::cout << it.elem << "\t";
		it = acp_increment_list_it(it);
	}
	std::cout << std::endl;
	return mark;
}

void remove_node(acp_list_t l, const int n ){
	std::cout << "-------- remove mark (" << n << ") --------\n";
	acp_list_it_t it = acp_begin_list(l); 
	while( it.list.ga != acp_end_list(l).list.ga ) {
		if( it.elem == n ) {
			it = acp_erase_list(it);
		}
		it = acp_increment_list_it(it);
	}
	it = acp_begin_list(l);
	while( it.list.ga != acp_end_list(l).list.ga ) {
		std::cout << it.elem << "\t";
		it = acp_increment_list_it(it);
	}
	std::cout << std::endl;
	return;
}

int main(int argc, char* argv[]){
	int i, len = 0;
	int *arr_p, *disp_p;
	char c, r = 0;
	unsigned short dt;

	acp_ch_t ch;
	acp_request_t req;
	acp_list_t seq;
	acp_list_it_t it;
	int procs, rank;

	acp_ga_t ga;
	acp_atkey_t key, atkey;
	acp_handle_t h;
	
	while ((c = getopt(argc, argv, "l:")) != -1){
		switch(c){
		 case 'l':
			len = atoi(optarg);
			if (len < 1){
				len = 1;
			}
			break;
		 default:
			std::cout << "usage: " << argv[0] << "[-l num]" << std::endl;
		}
	}
	acp_init(&argc, &argv);
	procs = acp_procs();
	rank = acp_rank();

	std::cout << "procs : " << procs << "  length of list : " << len  << std::endl;

	// element, handle(list): rank[0]
	if (rank == 0){
		arr_p = (int *)calloc(len, sizeof(int));
		key = acp_register_memory(arr_p, len*sizeof(int), 0);
		ga = acp_query_ga (key, arr_p);

		seq = acp_create_list(rank);
		it = acp_begin_list(seq);
		for( i = 0; i < len; i++ ){
			arr_p[i] = i+1;
			acp_push_back_list (seq, ga+i*sizeof(int), sizeof(int), rank);
			//if(it.elem == ACP_GA_NULL){
			//	std::cout  << "------ insert failed : " << i << " ------";
			//	continue;
			//}
		}

		i = 0;
		disp_p = (int *)calloc(1, sizeof(int) );
		for( it = acp_begin_list(seq);
			 it.elem != acp_end_list(seq).elem;
			 it = acp_increment_list_it (it) ) {
			disp_p = (int *)get_list_elem( it, disp_p, sizeof(int), atkey, h );
			fprintf(stderr, "[%d]%0d\t", i, *disp_p);
			i++;
		}
		std::cout << std::endl;

		int td = get_today();

		std::cout << "-------- find today--------" << "\n";
		for( it = acp_begin_list(seq);
			 it.elem != acp_end_list(seq).elem;
			 it = acp_increment_list_it (it) ) {
			disp_p = (int *)get_list_elem( it, disp_p, sizeof(int), atkey, h );
			if( *disp_p == td ) {
				std::cout << "<< " << *disp_p << ">> \t";
			}else{
				std::cout << *disp_p << "\t";
			}
		}

		std::cout << std::endl;
		acp_unregister_memory(key);
		free(disp_p);
		free (arr_p);

	}
	acp_finalize();
	return 0;
}
