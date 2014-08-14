#BLHEADERS = acpbl.h acpbl_udp.h acpbl_udp_gma.h acpbl_udp_gmm.h
BLHEADERS = acpbl.h 
#BLOBJS = acpbl_udp.o acpbl_udp_gma.o acpbl_udp_gmm.o
BLOBJS = acpbl_ib.o
#MLOBJS = acpdl.o acpvd.o
MLOBJS = 
CIOBJS = acpcl.o
CFLAGS = -I../../../src/include
OBJS = $(BLOBJS) $(CIOBJS) $(MLOBJS)
DEBUG= -DDEBUG
CC=gcc
#LIBS=-lpthread
LIBS=-lpthread -libverbs

.c.o: 
	$(CC) $(CFLAGS) $(DEBUG) -c $<

#all: acpbl_udp_test acpch_test0 acpch_test1 acpch_test2 acpch_test3 acpch_test4
all: acpch_test0 acpch_test1 acpch_test2 acpch_test3 acpch_test4

acpbl_udp_test: acpbl_udp_test.c $(OBJS)
	$(CC) $(CFLAGS) -o acpbl_udp_test $(DEBUG) acpbl_udp_test.c $(BLOBJS) $(LIBS)

acpch_test0: acpch_test0.c $(OBJS) 
	$(CC) $(CFLAGS) -o acpch_test0 $(DEBUG) acpch_test0.c $(OBJS) $(LIBS)

acpch_test1: acpch_test1.c $(OBJS) 
	$(CC) $(CFLAGS) -o acpch_test1 $(DEBUG) acpch_test1.c $(OBJS) $(LIBS)

acpch_test2: acpch_test2.c $(OBJS) 
	$(CC) $(CFLAGS) -o acpch_test2 $(DEBUG) acpch_test2.c $(OBJS) $(LIBS)

acpch_test3: acpch_test3.c $(OBJS) 
	$(CC) $(CFLAGS) -o acpch_test3 $(DEBUG) acpch_test3.c $(OBJS) $(LIBS)

acpch_test4: acpch_test4.c $(OBJS) 
	$(CC) $(CFLAGS) -o acpch_test4 $(DEBUG) acpch_test4.c $(OBJS) $(LIBS)

#acpbl_udp.o: $(BLHEADERS)
#acpbl_udp_gma.o: $(BLHEADERS)
#acpbl_udp_gmm.o: $(BLHEADERS)
acpbl_udp.o:
	cp ../../../src/bl/udp/acpbl_udp.o .
acpbl_udp_gma.o:
	cp ../../../src/bl/udp/acpbl_udp_gma.o .
acpbl_udp_gmm.o:
	cp ../../../src/bl/udp/acpbl_udp_gmm.o .
acpcl.o:
	cp ../../../src/ml/cl/acpcl.o .
acpdl.o:
	cp ../../../src/ml/dl/acpdl.o .
acpvd.o:
	cp ../../../src/ml/vd/acpvd.o .
acpbl_ib.o:
	cp ../../../src/bl/ib/acpbl_ib.o .

clean:
	/bin/rm -f acpbl_udp_test acpch_test0 acpch_test1 acpch_test2 acpch_test3 acpch_test4 *.o


