BLHEADERS = acpbl.h acpbl_sync.h acpbl_udp.h acpbl_udp_gma.h acpbl_udp_gmm.h acpch.h
BLOBJS = acpbl_udp.o acpbl_udp_gma.o acpbl_udp_gmm.o
CIOBJS = acpch.o
OBJS = $(BLOBJS) $(CIOBJS)
DEBUG= -DDEBUG
CC=gcc
LIBS=-lpthread

.c.o: 
	$(CC) $(DEBUG) -c $<

all: acpbl_udp_test acpch_test0 acpch_test1 acpch_test2 acpch_test3 acpch_test4

acpbl_udp_test: acpbl_udp_test.c $(OBJS)
	$(CC) -o acpbl_udp_test $(DEBUG) acpbl_udp_test.c $(BLOBJS) $(LIBS)

acpch_test0: acpch_test0.c $(OBJS) 
	$(CC) -o acpch_test0 $(DEBUG) acpch_test0.c $(OBJS) $(LIBS)

acpch_test1: acpch_test1.c $(OBJS) 
	$(CC) -o acpch_test1 $(DEBUG) acpch_test1.c $(OBJS) $(LIBS)

acpch_test2: acpch_test2.c $(OBJS) 
	$(CC) -o acpch_test2 $(DEBUG) acpch_test2.c $(OBJS) $(LIBS)

acpch_test3: acpch_test3.c $(OBJS) 
	$(CC) -o acpch_test3 $(DEBUG) acpch_test3.c $(OBJS) $(LIBS)

acpch_test4: acpch_test4.c $(OBJS) 
	$(CC) -o acpch_test4 $(DEBUG) acpch_test4.c $(OBJS) $(LIBS)

acpbl_udp.o: $(BLHEADERS)
acpbl_udp_gma.o: $(BLHEADERS)
acpbl_udp_gmm.o: $(BLHEADERS)

clean:
	/bin/rm -f acpbl_udp_test acpch_test0 acpch_test1 acpch_test2 acpch_test3 acpch_test4 *.o


