# coding: utf-8
import re
import math

class fastaRecord():
    def __init__(self, acc, head, seq):
        self.accession = acc
        self.header = head
        self.sequence = seq

def read_sequence_from_file(filename):
    pass

def revcomp(str):
    revstring = ""
    tbl = { 'g' :'c', 'G':'C', 'a':'t', 'A':'T', 't':'a', 'T':'A', 'c':'g', 'C':'G'}
    for i in len(str):
        j = len(str) - i - 1
        c = str[j]
        revchar = tbl.get(c, 'N')
        revstring = revstring + revchar

    return revstring

def readNextFastaRecord(reader):
    pass

def contains_non_gatc(kmer):
    rx = re.compile(".*[^GATCgatc]")
    m = rx.match(kmer)

    if m:
        return True
    else:
        return False

def int_to_base(baseval):
    pass

def kmer_to_intval(kmer):

    base_to_int = { 'G': 0, 'g':0,
                    'A': 1, 'g':1,
                    'T': 2, 'g':2,
                    'C': 3, 'g':3  }

    if len(kmer) > 32:
        raise "error, kmer length exceeds 32"

    kmer_val = 0

    for c in kmer:
        try:
            val = base_to_int[c]
        except:
            raise Exception("error, kmer contains nongatc: " + kmer)

	kmer_val = kmer_val << 2
	kmer_val = kmer_val | val
  
    return kmer_val

def decode_kmer_from_intval(intval, kmer_length):
    kmer = ""
    nucs = ['G', 'A', 'T', 'C']

    for i in range(kmer_length):
        base_num = intval & 3
        kmer = nucs[base_num] + kmer
        intval = intval >> 2

    return kmer

def kmerstr_to_colored_kmer(kmerstr):
    rv = ""
    for c in kmerstr:
        if c == 'A':
            rv = rv + "\033[031mA\033[0m"
        elif c == 'C':
            rv = rv + "\033[032mC\033[0m"
        elif c == 'G':
            rv = rv + "\033[033mG\033[0m"
        elif c == 'T':
            rv = rv + "\033[034mT\033[0m"
    return rv

def revcomp_val(kmer, kmer_length):
    rev_kmer = 0
    kmer = ~kmer
    for i in range(kmer_length):
        base = kmer & 3
        rev_kmer = rev_kmer << 2
        rev_kmer = rev_kmer + base
        kmer = kmer >> 2

    return rev_kmer

def get_DS_kmer_val(kmer, kmer_length):
    rev_kmer = revcomp_val(kmer_val, kmer_length)
    
    if rev_kmer > kmer_val:
        kmer_val = rev_kmer
    
    return kmer_val

def sequence_string_to_kmer_int_type_vector(seq, kmer_length):
    pass

def compute_entropy_val(kmer, kmer_length):
    counts = [ 0, 0, 0, 0 ]
    for i in range(kmer_length):
        c = kmer & 3
        kmer = kmer >> 2
        counts[c] = counts[c] + 1
    entropy = 0
    for i in range(4):
        prob = float(counts[i]) / kmer_length

	if prob > 0:
            val = prob * math.log(1/prob)/math.log(2.0)
            entropy = entropy + val
    return entropy

def compute_entropy(kmer):
    counts = [0,0,0,0]
    nucs   = {'G':0, 'A':1, 'T':2, 'C':3}
    for i in len(kmer):
        idx = nucs[kmer[i].uppercase()]
        counts[idx] = counts[idx] + 1

    entropy = 0.0;

    for i in 4:
        prob = float(counts[i]) / float(len(kmer))
        if prob > 0:
            val = prob * math.log(1/prob, 2.0)
            entropy = entropy + val

    return entropy

def replace_nonGATC_chars_with_A(seq):
    pass

def base_to_int_value(nucleotide):
    nucs   = {'G':0, 'A':1, 'T':2, 'C':3}
    if nucleotide in nucs:
        return nucs[nucleotide]
    else:
        return -1

def generateHash(s):
    # adapted from: http://stackoverflow.com/questions/8094790/how-to-get-hash-code-of-a-string-in-c
    combined_hashcode = 0
    hash_ = 0
    for nucleotide in s:
        hash_ = 65599 * hash_ + ord(nucleotide)

        hash_ = 0xFFFFFFFF & hash_

        # adding one just in case, since non-gatc = -1, but shouldn't encounter non-gatc here anyway.
        base_val = base_to_int_value(nucleotide) + 1

        combined_hashcode = combined_hashcode + base_val
        combined_hashcode = 0xFFFFFFFFFFFFFFFF & combined_hashcode

    hash_ = hash_ ^ (hash_ >> 16)
    combined_hashcode = combined_hashcode << 32
    combined_hashcode = combined_hashcode | hash_

    return combined_hashcode
