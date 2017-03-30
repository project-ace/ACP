# coding:utf-8
import sys
import os
import argparse
from ctypes import *

# Represents that no address translation key is available.
ATKEY_NULL  = 0x0000000000000000

# Null address of the global memory.
ACP_GA_NULL = 0x0000000000000000

# Represents all of the handles of GMAs that have been invoked so far.
HANDLE_ALL  = 0xffffffffffffffff

# Represents the continuation of the previous GMA.(*).
HANDLE_CONT = 0xfffffffffffffffe

# Represents that no handle is available.
HANDLE_NULL = 0x0000000000000000

libacpbl = None
libacpml = None

def init(ndev):
    global libacpbl
    global libacpml

    if ndev == 'udp':
        libacpbl = CDLL("libacpbl_udp.so.2",mode=1|256)
    elif ndev == 'ib':
        libacpbl = CDLL("libacpbl_ib.so.2",mode=1|256)
    else:
        raise RuntimeError("no dll for device" + ndev)

    libacpbl.acp_init.restype = c_int
    libacpbl.acp_init.argtypes = [c_void_p, c_void_p]

    libacpbl.acp_query_starter_ga.restype = c_ulonglong
    libacpbl.acp_query_starter_ga.argtypes = [c_int]

    libacpbl.acp_register_memory.restype = c_ulonglong
    libacpbl.acp_register_memory.argtypes = [c_void_p, c_uint, c_int]

    libacpbl.acp_unregister_memory.restype = c_int
    libacpbl.acp_unregister_memory.argtypes = [c_ulonglong]

    libacpbl.acp_query_ga.restype = c_ulonglong
    libacpbl.acp_query_ga.argtypes = [c_ulonglong, c_void_p]

    libacpbl.acp_query_rank.restype = c_int
    libacpbl.acp_query_rank.argtypes = [c_ulonglong]

    libacpbl.acp_query_address.restype = c_void_p
    libacpbl.acp_query_address.argtypes = [c_ulonglong]

    libacpbl.acp_copy.restype = c_ulonglong
    libacpbl.acp_copy.argtypes = [c_ulonglong, c_ulonglong, c_uint, c_ulonglong]

    libacpbl.acp_cas4.restype = c_ulonglong
    libacpbl.acp_cas4.argtypes = [c_ulonglong, c_ulonglong, c_uint32, c_uint32, c_ulonglong]

    libacpbl.acp_cas8.restype = c_ulonglong
    libacpbl.acp_cas8.argtypes = [c_ulonglong, c_ulonglong, c_uint64, c_uint64, c_ulonglong]

    libacpbl.acp_add8.restype = c_ulonglong
    libacpbl.acp_add8.argtypes = [c_ulonglong, c_ulonglong, c_uint64, c_ulonglong]

    libacpbl.acp_complete.argtypes = [c_ulonglong]

    libacpbl.acp_inquire.restype = c_int
    libacpbl.acp_inquire.argtypes = [c_ulonglong]

    libacpml = CDLL("libacpml.so.2",mode=1|256)

    libacpml.acp_clear_map.restype = None
    libacpml.acp_clear_map.argtypes = [map_t]

    libacpml.acp_create_map.restype = map_t
    libacpml.acp_create_map.argtypes = [c_int, c_void_p, c_int, c_int]

    libacpml.acp_size_local_map.restype = c_size_t
    libacpml.acp_size_local_map.argtypes = [map_t]

    libacpml.acp_insert_map.restype = c_int
    libacpml.acp_insert_map.argtypes = [map_t, pair_t]

    libacpml.acp_remove_map.restype = None
    libacpml.acp_remove_map.argtypes = [map_t, element_t]

    libacpml.acp_begin_map.restype = map_it_t
    libacpml.acp_begin_map.argtypes = [map_t]

    libacpml.acp_end_map.restype = map_it_t
    libacpml.acp_end_map.argtypes = [map_t]
    
    libacpml.acp_find_map.restype = map_it_t
    libacpml.acp_find_map.argtypes = [map_t, element_t]
    
    libacpml.acp_retrieve_map.restype = c_size_t
    libacpml.acp_retrieve_map.argtypes = [map_t, pair_t]
    
    libacpml.acp_dereference_map_it.restype = pair_t
    libacpml.acp_dereference_map_it.argtypes = [map_it_t]
    
    libacpml.acp_increment_map_it.restype = map_it_t
    libacpml.acp_increment_map_it.argtypes = [map_it_t]
    
    libacpml.acp_begin_multiset.restype = multiset_it_t
    libacpml.acp_begin_multiset.argtypes = [multiset_t]
    
    libacpml.acp_end_multiset.restype = multiset_it_t
    libacpml.acp_end_multiset.argtypes = [multiset_t]
    
    libacpml.acp_begin_local_multiset.restype = multiset_it_t
    libacpml.acp_begin_local_multiset.argtypes = [multiset_t]
    
    libacpml.acp_end_local_multiset.restype = multiset_it_t
    libacpml.acp_end_local_multiset.argtypes = [multiset_t]

    libacpml.acp_create_multiset.restype = multiset_t
    libacpml.acp_create_multiset.argtypes = [c_int, c_void_p, c_int, c_int]

    libacpml.acp_destroy_multiset.restype = None
    libacpml.acp_destroy_multiset.argtypes = [multiset_t]

    libacpml.acp_insert_multiset.restype = c_int
    libacpml.acp_insert_multiset.argtypes = [multiset_t, element_t]

    libacpml.acp_remove_multiset.restype = None
    libacpml.acp_remove_multiset.argtypes = [multiset_t, element_t]

    libacpml.acp_retrieve_multiset.restype = c_uint64
    libacpml.acp_retrieve_multiset.argtypes = [multiset_t, element_t]

    libacpml.acp_size_local_multiset.restype = c_size_t
    libacpml.acp_size_local_multiset.argtypes = [multiset_t]

    libacpml.acp_dereference_multiset_it.restype = element_t
    libacpml.acp_dereference_multiset_it.argtypes = [multiset_it_t]

    libacpml.acp_increment_multiset_it.restype = multiset_it_t
    libacpml.acp_increment_multiset_it.argtypes = [multiset_it_t]

    libacpml.acp_malloc.restype = c_ulonglong
    libacpml.acp_malloc.argtypes = [c_size_t, c_int]

    libacpml.acp_free.restype = None
    libacpml.acp_free.argtypes = [c_uint64]

    argc = len(sys.argv)
    argv = (c_char_p * argc)()
    argv_p = pointer(argv)
    argv_pp = pointer(argv_p)


    buffers = []
    for i in range(len(sys.argv)):
        buffers.append(create_string_buffer(sys.argv[i]))

    for i in range(len(sys.argv)):
        argv[i] = cast(pointer(buffers[i]), c_char_p)
        # print(buffers[i], argv[i], "%x" % addressof(buffers[i]))

    c_argc = c_int(argc)
    # print c_argc, argv_pp

    return libacpbl.acp_init(byref(c_argc), argv_pp)

def finalize():
    global libacpbl
    return libacpbl.acp_finalize()

def sync():
    global libacpbl
    libacpbl.acp_sync()

def rank():
    global libacpbl
    return libacpbl.acp_rank()

def procs():
    global libacpbl
    return libacpbl.acp_procs()

def query_starter_ga(rank):
    global libacpbl
    return libacpbl.acp_query_starter_ga(rank)

def register_memory(addr, size, color):
    global libacpbl
    return libacpbl.acp_register_memory(addr, size, color)

def unregister_memory(atkey):
    global libacpbl
    return libacpbl.acp_unregister_memory(atkey)

def query_ga(key, addr):
    global libacpbl
    return libacpbl.acp_query_ga(key, addr)

def query_rank(ga):
    global libacpbl
    return libacpbl.acp_query_rank(ga)

def query_address(ga):
    global libacpbl
    return libacpbl.acp_query_address(ga)

def copy(dst, src, size, order=HANDLE_NULL):
    global libacpbl
    #print("ACP.copy 0x%x(@%d) <= 0x%x(@%d), size=%d, order=0x%x" % (dst, query_rank(dst), src, query_rank(src), size, order))
    #sys.stdout.flush()
    return libacpbl.acp_copy(dst, src, size, order)

def cas4(dst, src, oldval, newval, order=HANDLE_NULL):
    global libacpbl
    return libacpbl.acp_cas4(dst, src, oldval, newval, order)

def cas8(dst, src, oldval, newval, order=HANDLE_NULL):
    global libacpbl
    return libacpbl.acp_cas8(dst, src, oldval, newval, order)

def add8(dst, src, value, order=HANDLE_NULL):
    global libacpbl
    return libacpbl.acp_add8(dst, src, value, order)

def complete(handle=HANDLE_ALL):
    global libacpbl
    #print("ACP.complete handle=0x%x" % handle)
    sys.stdout.flush()
    libacpbl.acp_complete(handle)

def inquire(handle):
    global libacpbl
    return libacpbl.acp_inquire(handle)

class map_t(Structure):
    _fields_ = [('ga', c_uint64), ('num_ranks', c_uint64), ('num_slots', c_uint64)]

class element_t(Structure):
    _fields_ = [('ga', c_uint64), ('size', c_size_t)]

class pair_t(Structure):
    _fields_ = [('first', element_t), ('second', element_t)]

class map_it_t(Structure):
    _fields_ = [('map', map_t), ('rank', c_int), ('slot', c_int), ('elem', c_uint64)]

class map_ib_t(Structure):
    _fields_ = [('it', map_it_t), ('success', c_int)]

class multiset_t(Structure):    
    _fields_ = [('ga', c_uint64), ('num_ranks', c_uint64), ('num_slots', c_uint64)]

class multiset_it_t(Structure):
    _fields_ = [('set', multiset_t), ('rank', c_int), ('slot', c_int), ('elem', c_uint64)]

def clear_map(map):
    global libacpml
    libacpml.acp_clear_map(map)

def create_map(num_ranks, ranks, num_slots, rank):
    global libacpml
    
    buf = (c_int * num_ranks)()
    for i in range(len(ranks)):
        buf[i] = ranks[i]

    return libacpml.acp_create_map(num_ranks, cast(buf, POINTER(c_int)), num_slots, rank)

def size_local_map(map):
    print "HOGE"
    global libacpml
    return libacpml.acp_size_local_map(map)

def insert_map(map, pair):
    global libacpml
    return libacpml.acp_insert_map(map, pair)

def remove_map(map, key):
    global libacpml
    libacpml.acp_remove_map(map, key)

def begin_map(map):
    global libacpml
    return libacpml.acp_begin_map(map)

def end_map(map):
    global libacpml
    return libacpml.acp_end_map(map)

def find_map(map, key):
    global libacpml
    return libacpml.acp_find_map(map, key)

def retrieve_map(map, pair):
    global libacpml
    print "retrieve_map"
    return libacpml.acp_retrieve_map(map, pair)

def dereference_map_it(map_it):
    global libacpml
    return libacpml.acp_dereference_map_it(map_it)

def increment_map_it(map_it):
    global libacpml
    return libacpml.acp_increment_map_it(map_it)

def begin_multiset(multiset):
    global libacpml
    return libacpml.acp_begin_multiset(multiset)

def end_multiset(multiset):
    global libacpml
    return libacpml.acp_end_multiset(multiset)

def begin_local_multiset(multiset):
    global libacpml
    return libacpml.acp_begin_local_multiset(multiset)

def end_local_multiset(multiset):
    global libacpml
    return libacpml.acp_end_local_multiset(multiset)

def create_multiset(num_ranks, ranks, num_slots, rank):
    global libacpml
    
    buf = (c_int * num_ranks)()
    for i in range(len(ranks)):
        buf[i] = ranks[i]
    return libacpml.acp_create_multiset(num_ranks, cast(buf, POINTER(c_int)), num_slots, rank)

def destroy_multiset(multiset):
    global libacpml
    libacpml.acp_destroy_multiset(multiset)

def insert_multiset(multiset, key):
    global libacpml
    return libacpml.acp_insert_multiset(multiset, key)

def remove_multiset(multiset, key):
    global libacpml
    libacpml.acp_remove_multiset(multiset, key)

def retrieve_multiset(multiset, key):
    global libacpml
    return libacpml.acp_retrieve_multiset(multiset, key)

def size_local_multiset(multiset):
    global libacpml
    return libacpml.acp_size_local_multiset(multiset)

def dereference_multiset_it(multiset_it):
    global libacpml
    return libacpml.acp_dereference_multiset_it(multiset_it)

def increment_multiset_it(multiset_it):
    global libacpml
    return libacpml.acp_increment_multiset_it(multiset_it)

def malloc(size, rank):
    global libacpml
    ga = libacpml.acp_malloc(size, rank)
    if ga == ACP_GA_NULL:
        raise Exception("acp_malloc returned ACP_GA_NULL")
    return ga

def free(ga):
    global libacpml
    libacpml.acp_free(ga)

# libacpml.acp_create_ch.restype = ch_t
# libacpml.acp_create_ch.argtypes = [int, int]
# def create_ch(sender, receiver):
#     global libacpml
#     return libacpml.acp_create_ch(sender, receiver)

# libacpml.acp_free_ch.restype = int
# libacpml.acp_free_ch.argtypes = [ch_t]
# def free_ch(ch):
#     global libacpml
#     libacpml.acp_free_ch(ch)

def create_elem8(value):
    #print "create_elem", value
    val_ga = malloc(8, rank())
    val_ptr = query_address(val_ga)
    cast(val_ptr, POINTER(c_int64))[0] = c_int64(value)
    return element_t(val_ga, 8)

def create_pair8(key, value):
    #print "create_pair", key, value
    return pair_t(create_elem8(key), create_elem8(value))

    # key_ga = malloc(8, rank())
    # val_ga = malloc(8, rank())

    # key_ptr = query_address(key_ga)
    # val_ptr = query_address(val_ga)
    
    # cast(key_ptr, POINTER(c_int64))[0] = c_long(key)
    # cast(val_ptr, POINTER(c_int64))[0] = c_long(value)

    # return pair_t(element_t(key_ga, 8), element_t(val_ga, 8))

def free_elem(elem):
    #print "free_elem", elem, elem.ga
    free(elem.ga)

def free_pair(pair):
    #print "free_pair", pair
    free_elem(pair.first)
    free_elem(pair.second)

    # free(pair.first.ga)
    # free(pair.second.ga)

def pretty_print(structure, level):
    for field_name, field_type in structure._fields_:
        print "  "*level + field_name, getattr(structure, field_name)
        if field_name == 'ga':
            ga = getattr(structure, field_name)
            ptr = query_address(ga)
            print "  "*level + "contents:" + str(cast(ptr, POINTER(c_uint64)).contents)

        if isinstance(getattr(structure, field_name), Structure):
            pretty_print(getattr(structure, field_name), level+1)

class Lock():
    def __init__(self, remote_ga=None):
        if remote_ga:
            self.lock_ga = remote_ga
        else:
            self.lock_ga = malloc(8, rank())
            ptr = query_address(self.lock_ga)
            cast(ptr, POINTER(c_uint64))[0] = 0

        self.local_buf_ga = malloc(8, rank())
        self.local_buf    = cast(query_address(self.local_buf_ga), POINTER(c_uint64))
        self.local_buf[0] = 0xdeadbeef
        self.locked = False
        print "lock init: lock_ga=%x, buf_ga=%x" % (self.lock_ga, self.local_buf_ga)

    def acquire(self):
        if self.locked:
            return

        #print "now acquiring..."

        while self.local_buf[0] != 0:
            #print "try cas8"
            h = cas8(self.local_buf_ga, self.lock_ga, 0, 1)
            complete(h)
            #print "self.local_buf[0] = %d" % (self.local_buf[0],)

        self.locked = True

    def free(self):
        #print "free lock"
        if not self.locked:
            return

        self.local_buf[0] = 0
        h = copy(self.lock_ga, self.local_buf_ga, 4)
        complete(h)

        self.local_buf[0] = 0xdeadbeef
        self.locked = False

class MultisetItemsIterator():
    def __init__(self, singlenodemultiset):
        self.multiset = singlenodemultiset.multiset
        self.it  = begin_local_multiset(self.multiset)
        self.end = end_local_multiset(self.multiset)

    def __iter__(self):
        return self

    def next(self):
        if self.it.elem == self.end.elem:
            raise StopIteration
        elem = dereference_multiset_it(self.it)
        key = cast(query_address(elem.ga), POINTER(c_int64)).contents.value
        self.it = increment_multiset_it(self.it)
        return key

class MapItemsIterator():
    def __init__(self, singlenodemap):
        self.map = singlenodemap.map
        self.it  = begin_map(self.map)
        self.end = end_map(self.map)

    def __iter__(self):
        return self

    def next(self):
        if self.it.elem == self.end.elem:
            raise StopIteration
        pair = dereference_map_it(self.it)
        key = cast(query_address(pair.first.ga), POINTER(c_int64)).contents.value
        val = cast(query_address(pair.second.ga), POINTER(c_int64)).contents.value
        #print "key = %d, val = %d" % ((key), long(val))
        self.it = increment_map_it(self.it)
        return (key, val)

class SingleNodeMultiset():

    def __init__(self, remote_ga=None, num_slots=2000000):
        print "SingleNodeMultiset()"
        if remote_ga:
            self.multiset = multiset_t(remote_ga, 1, num_slots)
        else:
            self.multiset = create_multiset(1, [rank()], num_slots, rank())

        self.tmp_buf_ga = malloc(8, rank())

    def ga2uint64(self, ga):
        garank = query_rank(ga)
        if garank == rank():
            val = cast(query_address(ga), POINTER(c_int64)).contents.value
        else:
            copy(self.tmp_buf_ga, ga, 8)
            complete()
            val = cast(query_address(self.tmp_buf_ga), POINTER(c_int64)).contents.value
        return val

    def __setitem__(self, key, value):
        #print "SingleNodeMultiset.setitem(%d, %d)" % (key, value)
        key_elem = create_elem8(key)
        for i in range(value):
            rv = insert_multiset(self.multiset, key_elem)
            if rv == 0:
                print "aborting..."
                sys.exit(-1)
            #print "insert_multiset: rv = %d" % (rv,)
        free_elem(key_elem)

    def delete(self, key):
        key_elem = create_elem8(key)
        rv = retrieve_multiset(self.multiset, key_elem)
        #val = self.ga2uint64(key_elem.ga)
        for i in range(rv):
            remove_multiset(self.multiset, key_elem)
        return True

        # key_elem = create_elem8(key)
        # it = find_map(self.map, key_elem)
        # pair = dereference_map_it(it)
        # if pair.second.ga != ACP_GA_NULL:
        #     remove_map(self.map, key_elem)
        #     return True
        # else:
        #     return False

    def __getitem__(self, key):
        key_elem = create_elem8(key)
        rv = retrieve_multiset(self.multiset, key_elem)
        free_elem(key_elem)
        return rv

    def items(self):
        return MultisetItemsIterator(self)

    def size(self):
        return size_local_multiset(self.multiset)

    def atomic_add(self, ga, val):
        add8(self.tmp_buf_ga, ga, val, HANDLE_NULL)
        complete()
        return self.ga2uint64(self.tmp_buf_ga)

    def increment(self, key, diff):
        #sys.stdout.write( "SingleNodeMultiset.increment(%d, %d)\n" % (key, diff) )
        key_elem = create_elem8(key)
        for i in range(diff):
            rv = insert_multiset(self.multiset, key_elem)
            #print "insert_multiset: rv = %d" % (rv,)
            if rv == 0:
                print "aborting..."
                sys.exit(-1)
        free_elem(key_elem)

    def dump(self):
        it = begin_local_multiset(self.multiset)
        end = end_local_multiset(self.multiset)
        print it, it.elem
        print end, end.elem
        while it.elem != end.elem:
            elem = dereference_multiset_it(it)
            key = cast(query_address(elem.ga), POINTER(c_int64))
            print "*key = %d" % ((key.contents.value),)
            it = increment_multiset_it(it)

class SingleNodeMap():

    def __init__(self, remote_ga=None, num_slots=1000):
        print "SingleNodeMap()"
        if remote_ga:
            self.map = map_t(remote_ga, 1, num_slots)
        else:
            self.map = create_map(1, [rank()], num_slots, rank())

        self.tmp_buf_ga = malloc(8, rank())

    def ga2uint64(self, ga):
        garank = query_rank(ga)
        if garank == rank():
            val = cast(query_address(ga), POINTER(c_int64)).contents.value
        else:
            copy(self.tmp_buf_ga, ga, 8)
            complete()
            val = cast(query_address(self.tmp_buf_ga), POINTER(c_int64)).contents.value
        return val

    def __setitem__(self, key, value):
        #print "SingleNodeMap.setitem(%d, %d)" % (key, value)
        key_elem = create_elem8(key)
        #print "find"
        it = find_map(self.map, key_elem)
        pair = dereference_map_it(it)
        #print "pair.second.ga = %x" % (pair.second.ga,)
        if pair.second.ga != ACP_GA_NULL:
            remove_map(self.map, key_elem)
        #free_elem(key_elem)
        #pair = create_pair8(key, value) 
        pair = pair_t(key_elem, create_elem8(value))
        #print "insert_map"
        rv = insert_map(self.map, pair)
        #print "rv of insert_map: ", rv
        if rv == 0:
            raise Exception("insert_map failed: key=%d, value=%d" % (key, value))
        free_pair(pair)

    def b__getitem__(self, key):
        print "getitem(key=%d)" % (key,)
        pair = create_pair8(key, 0xdeadbeef)
        print pair
        sz = retrieve_map(self.map, pair)
        print "retrieved"
        if sz == 0:
            val =  0 # 例外を上げるべき？
        else:
            val = self.ga2uint64(pair.second.ga) # cast(query_address(pair.second.ga), POINTER(c_int64)).contents.value
        free_pair(pair)
        return val

    def delete(self, key):
        key_elem = create_elem8(key)
        it = find_map(self.map, key_elem)
        pair = dereference_map_it(it)
        if pair.second.ga != ACP_GA_NULL:
            remove_map(self.map, key_elem)
            return True
        else:
            return False

    def __getitem__(self, key):
        #sys.stdout.write( "SingleNodeMap.getitem()\n" )
        key_elem = create_elem8(key)
        #print "find_map"
        it = find_map(self.map, key_elem)
        free_elem(key_elem)
        pair = dereference_map_it(it)
        if pair.second.ga == ACP_GA_NULL:
            val = 0
            #print "Item Not Found"
        else:
            val = self.ga2uint64(pair.second.ga) # cast(query_address(pair.second.ga), POINTER(c_int64)).contents.value
            #print "ItemValue = %d" % (val,)
        return val

    def items(self):
        return MapItemsIterator(self)

    def size(self):
        print "SIZE!!!"
        return size_local_map(self.map)

    def atomic_add(self, ga, val):
        add8(self.tmp_buf_ga, ga, val, HANDLE_NULL)
        complete()
        return self.ga2uint64(self.tmp_buf_ga)

    def increment(self, key, diff):
        sys.stdout.write( "SingleNodeMap.increment(%d, %d)\n" % (key, diff) )
        key_elem = create_elem8(key)
        pair = create_pair8(key, diff)

        retry_count = 1000

        while True:
            it = find_map(self.map, key_elem)
            deref = dereference_map_it(it)
            if deref.second.ga != ACP_GA_NULL:
                addresult = self.atomic_add(deref.second.ga, diff)
                print "addresult = %d" % (addresult,)
                break
            else:
                rv = insert_map(self.map, pair)
                if rv == 1:
                    print "1st insert of (%d,%d)" % (key, diff)
                    break
                else:
                    pass
                    #print "retrying insert_map..."
                    #retry_count = retry_count - 1
                    #if retry_count == 0:
                    #    sys.exit(-1)

        free_elem(key_elem)
        free_pair(pair)

    def increment_(self, key, diff):
        sys.stdout.write( "SingleNodeMap.increment(%d, %d)\n" % (key, diff) )
        key_elem = create_elem8(key)

        it = find_map(self.map, key_elem)

        pair = dereference_map_it(it)
        if pair.second.ga == ACP_GA_NULL:
            #print "1st insert"
            val = diff
        else:
            val = self.ga2uint64(pair.second.ga) # cast(query_address(pair.second.ga), POINTER(c_int64)).contents.value
            #print "previous val: %d" % (val,)
            val = val + diff
            #print "remove_map"
            remove_map(self.map, key_elem)

        pair = pair_t(key_elem, create_elem8(val))
        rv = insert_map(self.map, pair)
        print "rv of insert_map(%d, %d): %d" % (key, val, rv)
        if rv == 0:
            raise Exception("insert_map failed: key=%d, val=%d" % (key, val))
        free_pair(pair)

    def dump(self):
        it = begin_map(self.map)
        end = end_map(self.map)
        while it.elem != end.elem:
            pair = dereference_map_it(it)
            key = cast(query_address(pair.first.ga), POINTER(c_int64))
            val = cast(query_address(pair.second.ga), POINTER(c_int64))
            print "*key = %d, *val = %d" % ((key.contents.value), long(val.contents.value))
            it = increment_map_it(it)
