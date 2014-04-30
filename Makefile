CC = gcc
LIB = -libverbs
SRCDIR = ./acp/bl/ib
TESTDIR = ./test/bl/ib
INCLUDEDIR = ./acp/include

acpbl:  acpbl.o acpbl_test.o
	$(CC) $(TESTDIR)/acpbl_test.o $(SRCDIR)/acpbl.o -o $(TESTDIR)/acpbl $(LIB)

acpbl.o:
	$(CC) -c -I$(INCLUDEDIR) $(SRCDIR)/acpbl.c -o $(SRCDIR)/acpbl.o

acpbl_test.o:
	$(CC) -c -I$(INCLUDEDIR) $(TESTDIR)/acpbl_test.c -o $(TESTDIR)/acpbl_test.o

clean: 
	rm -f $(SRCDIR)/*.o $(TESTDIR)/*.o $(TESTDIR)/acpbl $(TESTDIR)/*.log
