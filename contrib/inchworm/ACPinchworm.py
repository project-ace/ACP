#!/usr/bin/python
# coding: utf-8

import sys,os
import math
import IRKE_COMMON
import ACP
import ctypes
from Fasta_reader import *
from sequenceUtil import *
from datetime import datetime
import time
import argparse
import resource
import numpy as np

me = 0
TOKEN = ""

MIN_CONNECTIVITY_RATIO = 0.0
MIN_ASSEMBLY_LENGTH    = 0      # minimum length of an inchworm assembly for reporting.
MIN_ASSEMBLY_COVERAGE  = 2      # minimum average kmer coverage for assembly to be reported.
MIN_SEED_ENTROPY       = 1.5    # minimum entropy for a Kmer to seed in inchworm assembly construction.
MIN_SEED_COVERAGE      = 2      # minimum kmer coverage for a seed.
DOUBLE_STRANDED_MODE   = False  # strand-specific by default
WRITE_KMER_FILES       = False
MONITOR_MPI_COMMUNICATION = False

def main():

    global TOKEN
    global MIN_ASSEMBLY_LENGTH
    global MIN_ASSEMBLY_COVERAGE
    global MIN_CONNECTIVITY_RATIO
    global MIN_SEED_ENTROPY
    global MIN_SEED_COVERAGE
    global DOUBLE_STRANDED_MODE
    global WRITE_KMER_FILES
    global MONITOR_MPI_COMMUNICATION

    parser = argparse.ArgumentParser()

    parser.add_argument('--acp-myrank', action='store', dest='myrank')
    parser.add_argument('--acp-nprocs', action='store', dest='nprocs')
    parser.add_argument('--acp-taskid', action='store', dest='taskid')
    parser.add_argument('--acp-port-local', action='store', dest='port_local')
    parser.add_argument('--acp-port-remote', action='store', dest='port_remote')
    parser.add_argument('--acp-host-remote', action='store', dest='host_remote')
    parser.add_argument('--acp-size-smem', action='store', dest='size_smem')
    parser.add_argument('--acp-size-smem-cl', action='store', dest='size_smem_cl')
    parser.add_argument('--acp-size-smem-dl', action='store', dest='size_smem_dl')

    # required params
    parser.add_argument('--reads', action='store', dest='fasta_filename')
    parser.add_argument('--kmers', action='store', dest='fasta_filename')
    parser.add_argument('--token', action='store', dest='token')

    # optional args
    parser.add_argument('--K', action='store', dest='kmer_length')
    parser.add_argument('--minKmerCount', action='store', dest='min_kmer_count')
    parser.add_argument('--L', action='store', dest='min_assembly_length')

    parser.add_argument('--min_assembly_coverage', action='store', dest='min_assembly_coverage')
    parser.add_argument('--monitor', action='store', dest='monitor')
    parser.add_argument('--min_con_ratio', action='store', dest='min_con_ratio')
    parser.add_argument('--DS')
    parser.add_argument('--min_seed_entropy', action='store', dest='min_seed_entropy')
    parser.add_argument('--min_seed_coverage', action='store', dest='min_seed_coverage')
    parser.add_argument('--max_test_kmers', action='store', dest='max_test_kmers')
    parser.add_argument('--write_kmer_files')
    parser.add_argument('--keep_tmp_files')
    parser.add_argument('--no_prune_error_kmers')
    parser.add_argument('--min_ratio_non_error', action='store', dest='min_ratio_non_error')

    parser.add_argument('app_args', nargs='*')

    args = parser.parse_args()

    output = open("output.%s" % (args.myrank,), "w")
    os.dup2(output.fileno(), sys.stdout.fileno())
    output.close()

    errout = open("err.%s" % (args.myrank,), "w")
    os.dup2(errout.fileno(), sys.stderr.fileno())
    errout.close()

    if args.fasta_filename == '':
        print "Error, must specify --kmers or --reads"
        exit(4)

    kmer_length = 25
    if args.kmer_length:
        kmer_length = int(args.kmer_length)
        print "kmer length set to %d" % (kmer_length,)

    min_kmer_count = 1
    if args.min_kmer_count:
        min_kmer_count = int(args.min_kmer_count)

    MIN_ASSEMBLY_LENGTH = kmer_length
    if args.min_assembly_length:
        MIN_ASSEMBLY_LENGTH = int(args.min_assembly_length)

    if args.min_assembly_coverage:
        MIN_ASSEMBLY_COVERAGE = int(args.min_assembly_coverage)

    #if args.monitor:
    #    IRKE_COMMON::MONITOR = args.monitor

    if args.min_con_ratio:
        MIN_CONNECTIVITY_RATIO = float(args.min_con_ratio)

    if args.DS:
        DOUBLE_STRANDED_MODE = True

    if args.min_seed_entropy:
        MIN_SEED_ENTROPY = float(args.min_seed_entropy)

    if args.min_seed_coverage:
        MIN_SEED_COVERAGE = int(args.min_seed_coverage)

    # some testing parameters.
    if args.max_test_kmers:
        MAX_TEST_KMERS = int(args.max_test_kmers)

    if args.write_kmer_files:
        WRITE_KMER_FILES = True

    if args.keep_tmp_files:
        KEEP_TMP_FILES = True

    # end of testing params
    
    prune_error_kmers = True
    # kmer error removal options
    if args.no_prune_error_kmers:
        prune_error_kmers = False

    min_ratio_non_error = 0.05
    if prune_error_kmers and args.min_ratio_non_error:
        min_ratio_non_error = float(args.min_ratio_non_error)

    ACP.init('udp') # ACP must be initialized before creating DistributedKmerCounter

    kcounter = DistributedKmerCounter(kmer_length, DOUBLE_STRANDED_MODE)
    read_file(args.fasta_filename, kcounter, kmer_length)

    if args.token:
        TOKEN = args.token
    
    ACP.sync()

    # next phase

    # k-kmer pruning
    if prune_error_kmers:
        do_prune_error_kmers(kcounter, min_ratio_non_error)

    do_assembly(kcounter, kmer_length)
    
    if ACP.rank() == 0:
        seen_contig_already = {} #map<unsigned long long, bool>
        
        INCHWORM_ASSEMBLY_COUNTER = 0

        assembly_start_node = 0
        assembly_end_node = ACP.procs() - 1

        for i in range(0, assembly_end_node+1):
            tmp_contig_file = get_ACP_proc_filename(i)
            sequence = ""
            tmpreader = open(tmp_contig_file, "r")

            for line in tmpreader:
                sequence = line.rstrip()

                contig_length = len(sequence)
                contig_hash = generateHash(sequence)
                
                if not contig_hash in seen_contig_already:
                    seen_contig_already[contig_hash] = True
                    INCHWORM_ASSEMBLY_COUNTER = INCHWORM_ASSEMBLY_COUNTER + 1
                    
                    header = ">a"     + str(INCHWORM_ASSEMBLY_COUNTER) \
                        + " K: "      + str(kmer_length) \
                        + " length: " + str(len(sequence))
                    
                    sequence = add_fasta_seq_line_breaks(sequence, 60)
                    
                    print header
                    print sequence
            
            tmpreader.close()

    print "DONE."

class Kmer_visitor():
    def __init__(self, kmer_length, is_ds):
        self.kmer_length = kmer_length
        self.ds_mode = is_ds
        self.set = []

    def add(self, kmer):

        if isinstance(kmer, str):
            kmer = kmer_to_intval(kmer)

        if self.ds_mode:
            kmer = get_DS_kmer_val(kmer, self.kmer_length)

        self.set.append(kmer)

    def exists(self, kmer):
        if isinstance(kmer, str):
            kmer = kmer_to_intval(kmer)

        if self.ds_mode:
            kmer = get_DS_kmer_val(kmer, self.kmer_length)

        if kmer in self.set:
            return True
        else:
            return False

    def erase(self, kmer):
        if isinstance(kmer, str):
            kmer = kmer_to_intval(kmer)

        if self.ds_mode:
            kmer = get_DS_kmer_val(kmer, self.kmer_length)

        if kmer in self.set:
            self.set.remove(kmer)

    def clear(self):
        self.set = []

    def size(self):
        return len(self.set)

class Segment():
    def __del__(self):
        ACP.unregister_memory(self.key)
   
    def __init__(self, ctype, size):
        self.type     = ctype
        self.size     = size
        self.typesize = ctypes.sizeof(ctype)
        self.bytesize = self.typesize * self.size
        self.buffer   = ctypes.create_string_buffer(self.bytesize)
        self.prettybuffer = ctypes.cast(self.buffer, ctypes.POINTER(ctype))
        self.key      = ACP.register_memory(ctypes.cast(self.buffer, ctypes.c_void_p), self.bytesize, 0)
        self.ga       = ACP.query_ga(self.key, ctypes.cast(self.buffer, ctypes.c_void_p))

    def __getitem__(self, key):
        return self.prettybuffer[key]
   
    def __setitem__(self, key, value):
        self.prettybuffer[key] = value

class DistributedKmerCounter():
    def __init__(self, kmer_length, ds_mode): # collective
        # key=64bit value=64bit
        self.kmer_length = kmer_length
        self.ds_mode = ds_mode

        self.multiset = ACP.SingleNodeMultiset()
        my_multiset_ga = self.multiset.multiset.ga
        print my_multiset_ga

        self.ga_buffer    = Segment(ctypes.c_ulonglong, ACP.procs())

        self.addcount = 0

        ACP.sync()

        local_starter_ga  = ACP.query_starter_ga(ACP.rank())
        starter_memory    = ACP.query_address(local_starter_ga)
        stmem_as_ull      = ctypes.cast(starter_memory, ctypes.POINTER(ctypes.c_ulonglong))
        stmem_as_ull[0]   = ctypes.c_ulonglong(my_multiset_ga)

        ACP.sync()

        for rank in range(ACP.procs()):
            ACP.copy(self.ga_buffer.ga + (rank * self.ga_buffer.typesize), ACP.query_starter_ga(rank), self.ga_buffer.typesize, ACP.HANDLE_NULL)

        ACP.complete()

        ACP.sync()

        # for rank in range(ACP.procs()):
        #     print "multiset's ga[%d]: %x" % (rank, self.ga_buffer[rank])

        self.remotemultiset = []
        for rank in range(ACP.procs()):
            if rank == ACP.rank():
                self.remotemultiset.append(self.multiset)
            else:
                self.remotemultiset.append(ACP.SingleNodeMultiset(self.ga_buffer[rank]))

        print self.remotemultiset

        ACP.sync()

        self.lock = ACP.Lock()
        stmem_as_ull[0] = ctypes.c_ulonglong(self.lock.lock_ga)
        
        ACP.sync()

        for rank in range(ACP.procs()):
            ACP.copy(self.ga_buffer.ga + (rank * self.ga_buffer.typesize), ACP.query_starter_ga(rank), self.ga_buffer.typesize, ACP.HANDLE_NULL)

        ACP.complete()
        
        ACP.sync()

        self.remotelock = []
        for rank in range(ACP.procs()):
            if rank == ACP.rank():
                self.remotelock.append(self.lock)
            else:
                self.remotelock.append(ACP.Lock(self.ga_buffer[rank]))

        print self.remotelock

    def size(self):
        return self.multiset.size()

    def get_kmer_string(self, kmer_val):
        return decode_kmer_from_intval(kmer_val, self.kmer_length)

    def get_contains_non_gatc(self, kmer):
        return contains_non_gatc(kmer)

    def get_kmer_intval(self, kmer):
        return kmer_to_intval(kmer)

    def get_kmer_length(self):
        return self.kmer_length
        
    def get_central_kmer(self, kmer):
        # given ABCDE, want BCD
        kmer = kmer >> 2 # remove last nucleotide
        kmer_mask = long(math.pow(2,2*( (self.kmer_length-1) -1) ) -1) # remove first nucleotide of the resulting (kmer-1) to get the core seq
        central_kmer = kmer & kmer_mask
        return central_kmer

    def get_central_right_kmer(self, kmer):
        # given ABCDE, want CDE
        kmer_mask = long(math.pow(2,2*(self.kmer_length-2))-1) # remove first two nucleotides of kmer
        central_kmer = kmer & kmer_mask
        return central_kmer

    def get_central_left_kmer(self, kmer):
        # given ABCDE, want ABC
        return kmer >> 4 # shift out the last two nucleotides.

    def get_node_for_central_kmer(self, central_kmer):
        #print "get_node_for_central_kmer(%d)" % (central_kmer,)

        canonical_central_kmer = central_kmer

        rev_central_kmer = revcomp_val(central_kmer, self.kmer_length-2)
    
        if rev_central_kmer < canonical_central_kmer:
            canonical_central_kmer = rev_central_kmer

        #print "canonical_central_kmer = %d" % (canonical_central_kmer,)

        node_for_kmer = canonical_central_kmer % ACP.procs()

        #print "node_for_kmer = %d" % (node_for_kmer,)
    
        if False: #IRKE_COMMON.MONITOR >= 4:
            print "Kmer: " + decode_kmer_from_intval(central_kmer, self.kmer_length-2) + " or " \
                + decode_kmer_from_intval(canonical_central_kmer, self.kmer_length-2) + " assigned to node: " + str(node_for_kmer)

        # all nodes are kmer servers
        return node_for_kmer
        
    def add_kmer(self, kmer, count):
        ck = self.get_central_kmer(kmer)
        node = self.get_node_for_central_kmer(ck)
        kmerstr = kmerstr_to_colored_kmer(decode_kmer_from_intval(kmer, self.kmer_length))
        ckmerstr = kmerstr_to_colored_kmer(decode_kmer_from_intval(ck,  self.kmer_length-2))
        #print "add_kmer(%s, %d): central_kmer = %s, node = %d" % (kmerstr, count, ckmerstr, node)
        #print "add_kmer(%s, %d) node = %d" % (kmerstr, count, node)

        if self.ds_mode:
            kmer = self.get_DS_kmer(kmer, self.kmer_length)

        self.addcount = self.addcount + 1
        if self.addcount % 1000 == 0:
            print "added %d kmers" % (self.addcount,)
        self.remotemultiset[node].increment(kmer, count)

    def find_kmer(self, kmer):
        if isinstance(kmer, str):
            kmer = self.get_kmer_intval(kmer)

        if self.ds_mode:
            kmer = self.get_DS_kmer(kmer, self.kmer_length)

        return self.multiset[kmer]

    def kmer_exists(self, kmer):
        if isinstance(kmer, str):
            kmer = self.get_kmer_intval(kmer)

        return self.get_kmer_count(kmer) > 0

    def get_kmer_count(self, kmer):
        if isinstance(kmer, str):
            kmer = get_kmer_intval(kmer)
        return self.multiset[kmer]

    def get_forward_kmer_candidates(self, seed_kmer):
        candidates = self.get_forward_kmer_candidates_unsorted(seed_kmer, False)
        #print candidates

        tmp = sorted(candidates, key=lambda x: x[1])
        candidates = tmp
        candidates.reverse()

        return candidates

    def get_forward_kmer_candidates_unsorted(self, seed_kmer, getZeros):
        forward_prefix = ((seed_kmer << (33-self.kmer_length) * 2) & 0xFFFFFFFFFFFFFFFF) >> (32 - self.kmer_length)*2

        candidates = []
        for i in range(4):
            k = forward_prefix | i
            ck = self.get_central_kmer(k)
            node = self.get_node_for_central_kmer(ck)
            #count = self.get_kmer_count(k)
            count = self.remotemultiset[node][k]
            if count > 0 or getZeros:
                candidates.append((k,count))
        return candidates

    def get_reverse_kmer_candidates(self, seed_kmer):
        candidates = self.get_reverse_kmer_candidates_unsorted(seed_kmer, False)

        tmp = sorted(candidates, key=lambda x: x[1])
        candidates = tmp
        candidates.reverse()

        return candidates

    def get_reverse_kmer_candidates_unsorted(self, seed_kmer, getZeros):
        reverse_suffix = seed_kmer >> 2
        candidates = [] #{}
        for i in range(4):
            k = (i << (self.kmer_length*2 - 2)) | reverse_suffix
            ck = self.get_central_kmer(k)
            node = self.get_node_for_central_kmer(ck)
            count = self.remotemultiset[node][k]
            if count > 0 or getZeros:
                candidates.append((k,count))
        return candidates

    def get_kmers_sort_descending_counts(self): # sort local kmers
        print "Getting vec of kmers"
        num_kmers = self.multiset.size()
        start = datetime.now()

        dtype = [('key', np.uint64), ('value', np.uint64)]
        kmer_arr  = np.empty([num_kmers], dtype=dtype)

        print "Kcounter hash size: %d" % (num_kmers,)

        count = 0

        for k in self.multiset.items():
            v = self.multiset[k]
            if v > 0:
                kmer_arr[count][0] = k
                kmer_arr[count][1] = v
                count = count + 1
                if count % 1000 == 0:
                    print "count=%d" % (count,)
                
        print "Processed %d non-zero abundance kmers in kcounter." % (count,)
    
        if IRKE_COMMON.DEVEL_no_kmer_sort:
            print "-Not sorting list of kmers, given parallel mode in effect."
            return kmer_list

        print "Sorting %d kmers ..." % (count,)

        kmer_arr[0:count].sort(order='value')

        end = datetime.now()

        time_spent = end - start
        print "Done sorting %d kmers, taking %s seconds." % (num_kmers, time_spent)

        # return reversed slice
        return kmer_arr[0:count][::-1]

    def clear_kmer(self, kmer):
        if isinstance(kmer, str):
            kmer = kmer_to_intval(kmer)

        ck = self.get_central_kmer(kmer)
        node = self.get_node_for_central_kmer(ck)
        return self.remotemultiset[node].delete(kmer)

    def prune_kmer_extensions(self, min_ratio_non_error):
        deletion_list = []
        for kmer_val in self.multiset.items():
            count = self.multiset[kmer_val]
            if count == 0:
                continue
            candidates = self.get_forward_kmer_candidates(kmer_val)
            dominant_count = 0
            for i in range(len(candidates)):
                if candidates[i][1]:
                    candidate_count = candidates[i][1]
                    if dominant_count == 0:
                        dominant_count = candidate_count
                    elif dominant_count > 0 and float(candidate_count)/float(dominant_count) < min_ratio_non_error:
                        kmer_candidate = self.find_kmer( candidates[i][0] )
                        deletion_list.append(kmer_candidate)
                        # kmer_candidate->second = 0; // disable when encountered in further iterations.

        if len(deletion_list) > 0:
            for kmer in deletion_list:
                self.prune_kmer(kmer)
            return True
        else:
            return False

    def dump(self):
        self.multiset.dump()

def get_central_kmer(kmer, kmer_length):
    # given ABCDE, want BCD
    kmer = kmer >> 2 # remove last nucleotide
    kmer_mask = long(math.pow(2,2*( (kmer_length-1) -1) ) -1) # remove first nucleotide of the resulting (kmer-1) to get the core seq
    central_kmer = kmer & kmer_mask
    return central_kmer

def get_central_right_kmer(kmer, kmer_length):
    # given ABCDE, want CDE
    kmer_mask = long(math.pow(2,2*(kmer_length-2))-1) # remove first two nucleotides of kmer
    central_kmer = kmer & kmer_mask
    return central_kmer

def get_central_left_kmer(kmer, kmer_length):
    # given ABCDE, want ABC
    return kmer >> 4 # shift out the last two nucleotides.

def get_node_for_central_kmer(central_kmer, kmer_length):
    canonical_central_kmer = central_kmer
    rev_central_kmer = revcomp_val(central_kmer, kmer_length)
    
    if rev_central_kmer < canonical_central_kmer:
        canonical_central_kmer = rev_central_kmer

    node_for_kmer = canonical_central_kmer % ACP.procs()
    
    if False: ##IRKE_COMMON.MONITOR >= 4:
        print "Kmer: " + decode_kmer_from_intval(central_kmer, kmer_length) + " or " \
             + decode_kmer_from_intval(canonical_central_kmer, kmer_length) + " assigned to node: " + str(node_for_kmer)

    # all nodes are kmer servers
    return node_for_kmer

def is_good_seed_kmer(kcounter, kmer, kmer_count, kmer_length, min_connectivity):

    print "kmer:" + str(kmer) + " kmer_count:" + str(kmer_count) + " kmer_length:" + str(kmer_length)

    if kmer_count == 0: return False

    if kmer == revcomp_val(kmer, kmer_length):
        # palindromic kmer, avoid palindromes as seeds
        if IRKE_COMMON.MONITOR >= 2:
            print "SEED kmer: " + kcounter.get_kmer_string(kmer) + " is palidnromic.  Skipping. " + "\n";
    
        return False

    if kmer_count < MIN_SEED_COVERAGE:
        if IRKE_COMMON.MONITOR >= 2:
            print "-seed has insufficient coverage, skipping"
        return False

    entropy = compute_entropy_val(kmer, kmer_length)
    
    
    if entropy < MIN_SEED_ENTROPY :
        if IRKE_COMMON.MONITOR >= 2:
            print "-skipping seed due to low entropy: " + str(entropy)
    
        return False
    # got this far, so kmer is fine as a seed
    return True

def build_inchworm_contig_from_seed(kmer, kcounter, min_connectivity): #, PARALLEL_IWORM):

    kmer_length = kcounter.get_kmer_length()

    # track those kmers included in growing path.
    visitor = Kmer_visitor(kmer_length, DOUBLE_STRANDED_MODE)

    forward_path = inchworm(kcounter, 'F', kmer, visitor, min_connectivity)

    ## visitor.clear()

    # add selected path to visitor

    if IRKE_COMMON.MONITOR >= 2:
        print "Forward path contains: " + str(len(forward_path)) + " kmers. "

        for kmer_ in forward_path:
            #visitor.add(kmer_)
            print "\tForward path kmer: " + kcounter.get_kmer_string(kmer_)
        
    ### Extend to the left ###
    # visitor.erase(kmer) # reset the seed
        
    reverse_path = inchworm(kcounter, 'R', kmer, visitor, min_connectivity)

    if IRKE_COMMON.MONITOR >= 2:
        print "Reverse path contains: " + str(len(reverse_path)) + " kmers. "
        for p in reverse_path:
            print "\tReverse path kmer: " + kcounter.get_kmer_string(p)

    joined_path = join_forward_n_reverse_paths(reverse_path, kmer, forward_path);

    return joined_path

def join_forward_n_reverse_paths(reverse_path, seed_kmer_val, forward_path):
    joined_path = []
    
    # want reverse path in reverse order
    for path in reversed(reverse_path):
        joined_path.append( path )
    
    # add seed kmer
    joined_path.append(seed_kmer_val)
    
    # tack on the entire forward path.
    for path in forward_path:
        joined_path.append( path )

    return joined_path

def inchworm(kcounter, direction, kmer, visitor, min_connectivity):
    growing_path = []
    kmer_length = kcounter.get_kmer_length()

    while True:
        if direction == 'F':
            # forward search
            kmer_candidates = kcounter.get_forward_kmer_candidates(kmer)
        else:
            # reverse search
            kmer_candidates = kcounter.get_reverse_kmer_candidates(kmer)

        print "kmer_candidates", kmer_candidates

        if len(kmer_candidates):
            best_extension = kmer_candidates[0][0]
        else:
            best_extension = 0

        print "best_extension", best_extension

        if best_extension == 0:
            break
        elif visitor.exists(best_extension):
            break
        else:
            visitor.add(best_extension)
            growing_path.append(best_extension)
            kmer = best_extension

    return growing_path

def reconstruct_path_sequence(kcounter, path):
    if len(path) == 0: return ""
    
    seq = kcounter.get_kmer_string(path[0])
    #cov_counter.append( kcounter.get_kmer_count(path[0]) )
    
    for kmer in path:
        kmer_str = kcounter.get_kmer_string(kmer)
        seq = seq + kmer_str[len(kmer_str) - 1:len(kmer_str)]
        #cov_counter.append(kcounter.get_kmer_count(kmer))

    return seq

def zap_kmers(kcounter, kmer_path):
    kmer_length = kcounter.get_kmer_length()

    # exclude kmers built into current contig.
    for kmer in kmer_path:
        kcounter.clear_kmer(kmer)

def run_MPI_master_all_completion_check(phase_val):
    # if not the master, just return...  
    if ACP.rank() != 0:
        return

    print "-running MPI master all completion check"

    if MONITOR_MPI_COMMUNICATION:
        filewriter = open("master.completion_check."+str(phase_val)+".log", "w")

    unfinished_nodes = {}
    
    for i in range(1,ACP.procs()): # do not include self (master) node - already know master is done.
        unfinished_nodes[i] = True

    buffer = [0,0,0,0,0]
               
    while len(unfinished_nodes) > 0:
        finished_nodes = []

        for (node_id,value) in unfinished_nodes.items():
            # check to see if it's finished.

            buffer[0] = GET_FINISHED_STATUS
            buffer[1] = 0                # init to false
            buffer[3] = node_id          # from
            buffer[4] = ACP.rank() # to
            
            print "asking node: " + str(node_id) + " if finished... "
            if MONITOR_MPI_COMMUNICATION:
                filewriter.write(">> asking GET_FINISHED_STATUS from node[" + node_id + "] \n")

            # require MPI call here to that node to ask if its done yet.
            MPI.COMM_WORLD.send(buffer, dest=node_id, tag=GENERAL_WORK_REQUEST)
            buffer = MPI.COMM_WORLD.recv(source=node_id, tag=GENERAL_WORK_RESPONSE)

            print "?? IS_FINISHED? from master to node[" + str(node_id) + "] is: " + str(buffer[1]) + ", response msg: " + enum_description[buffer[0]] \
                 + ", audit_from: " + str(buffer[3]) \
                 + ", audit to: " + str(buffer[4])

            if MONITOR_MPI_COMMUNICATION:
                filewriter.write("<< received GET_FINISHED_STATUS from node[" + str(node_id) + "] = " + enum_description[buffer[0]])

            if buffer[0] != SENDING_FINISHED_STATUS or buffer[0] == ERROR_ENCOUNTERED or buffer[3] != node_id or buffer[4] != ACP.rank():
                print "\n** ERROR ** getting finished status from node: " + str(node_id)
            else:
                if buffer[1] == 1:
                    # node is finished.
                    finished_nodes.append(node_id)

            print "\tchecking node: " + str(node_id) + " if finished: " + str(buffer_[1])
        
        for node in finished_nodes:
            unfinished_nodes.erase(node)

        if len(unfinished_nodes) != 0:
            print "resting between polls for " + MPI_SLEEP_INTERVAL
            sleep(MPI_SLEEP_INTERVAL)

        print "Number of unfinished nodes is currently: " + str(len(unfinished_nodes))
    
    # send any waiting nodes the 'all done' signal.
    buffer[0] = SET_ALL_NODES_DONE
    
    first_node = 0
    if (MASTER_SLAVE_MODE):
        first_node = 0 # master is not running a kmer server, so no need to send a msg to it.  probably doesnt matter though. # yes it is!!!

    # include msg to master itself since the other thread is likely waiting for a msg.
    for i in range(first_node, ACP.procs()):
        print "Signalling all done to node: " + str(i)
        buffer[3] = ACP.rank() # from
        buffer[4] = i                # to

        MPI.COMM_WORLD.send(buffer, dest=i, tag=GENERAL_WORK_REQUEST)

def test_MPI(kcounter):
    raise NotImplementedError("test_MPI is not implemented")

def add_fasta_seq_line_breaks(sequence, interval):
    fasta_seq = ""
    counter = 0

    for c in sequence:
        counter = counter + 1
        fasta_seq = fasta_seq + c
        if counter % interval == 0 and counter != len(sequence):
            fasta_seq = fasta_seq + '\n'

    return fasta_seq

def get_ACP_proc_filename(node_id):
    return "tmp." + TOKEN + ".iworm_acp_proc_" + str(node_id) + ".contigs.txt";

def extract_best_seed(kmer_vec, kcounter, min_connectivity):
    kmer_length = kcounter.get_kmer_length()
    best_kmer_count = 0
    best_seed = 0

    for kmer in kmer_vec:
        count = kcounter.get_kmer_count(kmer)
        if count > best_kmer_count and is_good_seed_kmer(kcounter, kmer, count, kmer_length, min_connectivity):
            best_kmer_count = count
            best_seed = kmer

    if IRKE_COMMON.MONITOR >= 2:
        print "Parallel method found better seed: " + kcounter.get_kmer_string(best_seed) + " with count: " + str(best_kmer_count)

    return best_seed

def read_file(fasta_filename, kcounter, kmer_length):
    if False: #ACP.rank() == 1:
        print "sleeping..."
        time.sleep(3600)
    else:
        print "reading file..."
    # everyone participates in reading a part of the kmer file
            
    # figure out which section of the kmer fasta file we're supposed to use:
    file_length = 0  # init, set below
    this_mpi_section_start = 0
    this_mpi_section_end   = -1
            
    if ACP.procs() > 1:
        fasta_file_reader = open(fasta_filename, "r")
        fasta_file_reader.seek(0, 2)
        file_length = fasta_file_reader.tell()
        fasta_file_reader.seek(0, 2)
        fasta_file_reader.close()

        mpi_section_length = file_length / ACP.procs()
        this_mpi_section_start = ACP.rank() * mpi_section_length;
        this_mpi_section_end = this_mpi_section_start + mpi_section_length
            
    #---------------------------------------------
    # Kmer partitioning among the nodes: 
    # Populate kmer hashtable on each node
    # where each node gets a subset of the kmers
    #---------------------------------------------
            
    fasta_reader = Fasta_reader(fasta_filename, this_mpi_section_start, this_mpi_section_end)
    
    if WRITE_KMER_FILES:
        filewriter = open("tmp." + TOKEN + ".kmers.tid_" + str(ACP.rank()), "w")

    if MONITOR_MPI_COMMUNICATION:
        kmerReaderLog = open("mpi_kmer_file_reader.mpi_" + str(ACP.rank()) + ".log", "w")
            
    kmer_counter = 0
    
    while True:

        if not fasta_reader.hasNext():
            break

        fe = fasta_reader.getNext()
        seq = fe.sequence
                
        if seq == "":
            continue
                
        if len(seq) < kmer_length:
            continue
                
        kmer_counter = kmer_counter + 1

        print "kmer_counter = %d\n" % (kmer_counter,)

        count = 1
        
        if False: #READ_TYPE == KMER:
            count = int(fe.get_header())

        sys.stdout.write("input kmer: " + seq + "\n")
                
        for i in range(len(seq) - kmer_length + 1):
            #print i
            kmer_s = seq[i:i+kmer_length] # seq.substr(i, kmer_length); 

            if contains_non_gatc(kmer_s):
                continue

            kmer = kcounter.get_kmer_intval(kmer_s)
            kcounter.add_kmer(kmer, count)
            
            central_kmer = get_central_kmer(kmer, kmer_length)
                
            if IRKE_COMMON.MONITOR >= 4:
                pass
                # central_kmer_string = decode_kmer_from_intval(central_kmer, kmer_length-2);
                # right_central_kmer = decode_kmer_from_intval(get_central_right_kmer(kmer, kmer_length), kmer_length-2)
                # left_central_kmer = decode_kmer_from_intval(get_central_left_kmer(kmer, kmer_length), kmer_length-2)
                # sys.stdout.write(  "central: kmer " + str(central_kmer_string) )
                # sys.stdout.write( " left central kmer: " + left_central_kmer )
                # sys.stdout.write( " right central kmer: " + right_central_kmer )

                # partition kmers according to central kmer value and thread number
                # so all kmers with common core sequence end up on the same thread.
                # note, by virtue of this, all extensions for a given kmer should be
                # accessible via the same node. (idea of Bill Long @ Cray)

    print "Node[" + str(ACP.rank()) + "] is Done populating kmers."
    if MONITOR_MPI_COMMUNICATION:
        kmerReaderLog.writeln("Node[" + str(ACP.rank()) + "] is Done populating kmers.")
            
    THIS_NODE_DONE = True

    if ACP.rank() == 0:
        if ACP.procs() == 1:
            # no reason to run kmer server
            print "** Phase 1: Only 1 MPI node, no reason to do MPI communication. Skipping run_MPI_master_all_completion_check())"
        else:
            print "Phase 1: Master node running MPI_completion check."
            #  Barrierで行けない理由がわからないので保留
            # run_MPI_master_all_completion_check(1)

def do_prune_error_kmers(kcounter, min_ratio_non_error):
    if ACP.rank() == 0:
       print "Kmer db size before pruning: " + str( kcounter.size() )

    kcounter.prune_kmer_extensions(min_ratio_non_error)
        
    if ACP.rank() == 0:
       print "Kmer db size after pruning: " + str( kcounter.size() )

    ACP.sync()

def do_assembly(kcounter, kmer_length):
    contig_outfilename = get_ACP_proc_filename(ACP.rank())
    contig_writer = open(contig_outfilename, "w")
    print "Writing contigs to: " + contig_outfilename

    kmers = kcounter.get_kmers_sort_descending_counts()

    for j in range(len(kmers)):
        kmer = long(kmers[j][0])
        kmer_count = kcounter.get_kmer_count(kmer)
        print "kmer=%s, count=%d" % (decode_kmer_from_intval(kmer,kmer_length), kmer_count)

        if not is_good_seed_kmer(kcounter, kmer, kmer_count, kmer_length, MIN_CONNECTIVITY_RATIO):
            continue

        # build draft contig.
        joined_path = build_inchworm_contig_from_seed(kmer, kcounter, MIN_CONNECTIVITY_RATIO)

        # now use this draft contig to select a new seed:
        new_seed = extract_best_seed(joined_path, kcounter, MIN_CONNECTIVITY_RATIO)
                    
        if new_seed == 0:
            continue # must have been zapped by another thread
            
        # nicely polished new inchworm contig.
        joined_path = build_inchworm_contig_from_seed(new_seed, kcounter, MIN_CONNECTIVITY_RATIO)
        sequence = reconstruct_path_sequence(kcounter, joined_path)
                                        
        contig_length = len(sequence)

        if contig_length >= MIN_ASSEMBLY_LENGTH:
            contig_writer.write(sequence+"\n");
                    
        # remove the kmers.
        zap_kmers(kcounter, joined_path)

    contig_writer.close()

    # all nodes run this.
    THIS_NODE_DONE = True
    print "NODE: " + str( ACP.rank() ) + " IS DONE ASSEMBLING.\n"
    ACP.sync()
    
main()
