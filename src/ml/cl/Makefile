BLHEADERS = acpbl.h acpbl_sync.h acpbl_udp.h acpbl_udp_gma.h acpbl_udp_gmm.h acpcl.h
BLOBJS = acpbl_udp.o acpbl_udp_gma.o acpbl_udp_gmm.o
CIOBJS = acpcl.o
OBJS = $(BLOBJS) $(CIOBJS)
DEBUG= -DDEBUG
CC=gcc
LIBS=-lpthread

.c.o: 
	$(CC) $(DEBUG) -c $<

all: acpbl_udp_test acpcl_test0 acpcl_test1 acpcl_test2 acpcl_test3 acpcl_test4

acpbl_udp_test: acpbl_udp_test.c $(OBJS)
	$(CC) -o acpbl_udp_test $(DEBUG) acpbl_udp_test.c $(BLOBJS) $(LIBS)

acpcl_test0: acpcl_test0.c $(OBJS) 
	$(CC) -o acpcl_test0 $(DEBUG) acpcl_test0.c $(OBJS) $(LIBS)

acpcl_test1: acpcl_test1.c $(OBJS) 
	$(CC) -o acpcl_test1 $(DEBUG) acpcl_test1.c $(OBJS) $(LIBS)

acpcl_test2: acpcl_test2.c $(OBJS) 
	$(CC) -o acpcl_test2 $(DEBUG) acpcl_test2.c $(OBJS) $(LIBS)

acpcl_test3: acpcl_test3.c $(OBJS) 
	$(CC) -o acpcl_test3 $(DEBUG) acpcl_test3.c $(OBJS) $(LIBS)

acpcl_test4: acpcl_test4.c $(OBJS) 
	$(CC) -o acpcl_test4 $(DEBUG) acpcl_test4.c $(OBJS) $(LIBS)

acpbl_udp.o: $(BLHEADERS)
acpbl_udp_gma.o: $(BLHEADERS)
acpbl_udp_gmm.o: $(BLHEADERS)

clean:
	/bin/rm -f acpbl_udp_test acpcl_test0 acpcl_test1 acpcl_test2 acpcl_test3 acpcl_test4 *.o


