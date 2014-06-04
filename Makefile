include ./Makefile.inc

all:
	cd $(SRCDIR) ; make
	cd $(TESTDIR) ; make

#install:
#	cp $(INCDIR)/acpbl.h $(INSTALLINCDIR)
#	cp $(LIB_VER_REAL) $(INSTALLLIBDIR)
#	ln -sf $(INSTALLLIBDIR)/$(LIB_VER) $(INSTALLLIBDIR)/$(LIB)

###ALL: acpbl acpbl_ohandle
###
###acpbl:  acpbl.o acpbl_test.o
###	$(CC) $(TESTDIR)/acpbl_test.o $(SRCDIR)/acpbl.o -o $(TESTDIR)/acpbl $(LIB)
###
###acpbl_ohandle:  acpbl.o acpbl_ohandle.o
###	$(CC) $(TESTDIR)/acpbl_ohandle.o $(SRCDIR)/acpbl.o -o $(TESTDIR)/acpbl_ohandle $(LIB)
###
###acpbl.o:
###	$(CC) -c -I$(INCLUDEDIR) $(SRCDIR)/acpbl.c -o $(SRCDIR)/acpbl.o
###
###acpbl_test.o:
###	$(CC) -c -I$(INCLUDEDIR) $(TESTDIR)/acpbl_test.c -o $(TESTDIR)/acpbl_test.o
###
###acpbl_ohandle.o:
###	$(CC) -c -I$(INCLUDEDIR) $(TESTDIR)/acpbl_test_order_handle.c -o $(TESTDIR)/acpbl_ohandle.o
###
###clean: 
###	rm -f $(SRCDIR)/*.o $(TESTDIR)/*.o $(TESTDIR)/acpbl $(TESTDIR)/*.log
