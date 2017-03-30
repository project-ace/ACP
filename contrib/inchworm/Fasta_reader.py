# coding: utf-8
from Fasta_entry import Fasta_entry

class Fasta_reader():
    def __init__(self, filename, start_reading = 0, end_reading = -1): # TODO: add long start_reading and end_reading
        self.end_reading = end_reading

        if filename == "-":
            filename = "/dev/fd/0" # read from stdin

        try:
            f = open(filename, "r")
            
            if start_reading > 0:
                f.seek(start_reading)

            # tmp = f.readline()
            while True:
                tmp = f.readline()
                self.lastline = tmp.rstrip()
                if self.lastline[0] == '>':
                    break
                    
            #print str(self)+".lastline=\""+ self.lastline + "\""
            self.file = f

        except IOError as e:
            raise BaseException("I/O error({0}): {1}".format(e.errno, e.strerror))

    def hasNext(self):
        if self.lastline:
            if self.end_reading > 0 and self.file.tell() >= self.end_reading: # bad:  this->_filereader.tellg() >= end_reading) <- why?
                # force it to go to the end of the file
                self.file.seek(0, 2)
                return False
            else:
                return True
        else:
            return False

    def getNext(self):
        header = self.lastline

        sequence = ""

        try:
            while True:
                tmp = self.file.readline()
                #print "len(tmp) = %d, tell() = %d" % (len(tmp),self.file.tell())
                self.lastline = tmp.rstrip()
                #print "lastline: %s" % (self.lastline,)
                if self.lastline[0] != '>':
                    sequence = sequence + self.lastline
                else:
                    break
                    
            # check if only reading section of a file
            if self.end_reading > 0 and this.file.tell() >= self.end_reading: # bad: this->_filereader.tellg() >= end_reading) {
                # force it to go to the end of the file
                self.file.seek(0, 2)
        except:
            pass
           
        fe = Fasta_entry(header, sequence)
        return fe

    def retrieve_all_seqa_hash(self):
        all_seqs_hash = {}
        while self.hasNext():
            f = self.getNext()
            all_reqs_hash[f.accession] = f.sequence

        return all_seqs_hash
