# coding: utf-8
import re

class Fasta_entry():
    def __init__(self, header, sequence):
        acc = ""
        if header[0] == '>':
            header = header[1:-1]

            toks = re.split(r'[ \t]', header)

            if len(toks) > 0:
                acc = toks[0]

        self.accession = acc
        self.header    = header
        self.sequence  = sequence
