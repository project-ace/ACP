# coding: utf-8
MAX_THREADS_READ_PARSING = 6  # going higher leads to decreased performance in Inchworm due to thread collisions.
NUM_THREADS = -1 # use OMP_NUM_THREADS by default.
KEEP_TMP_FILES = False

MONITOR = 5 # shared among rest of code via irke_common.hpp, but lives here.

# various devel params
DEVEL_no_kmer_sort       = False
DEVEL_no_greedy_extend   = False
DEVEL_no_tie_breaking    = False
DEVEL_zero_kmer_on_use   = False
DEVEL_rand_fracture      = False
DEVEL_rand_fracture_prob = 0 # set to probability of fracturing at any inchworm contig extension

