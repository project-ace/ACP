ACPDIR = /usr/local/acp
CC = $(ACPDIR)/bin/acpcc #-DDEBUG

SRCP = test-procinfo.c
EXEP = test-procinfo
SRCM = test-msg.c
EXEM = test-msg
LIBSRCP = procinfo.c
HEADERP =  procinfo.h
LIBSRCM = msg.c lock.c
HEADERM =  msg.h lock.h
EXE = $(EXEP) $(EXEM)

all: $(EXE) 

$(EXEP): $(SRCP) $(LIBSRCP) $(HEADERP)
	$(CC) -o $(EXEP) $(SRCP) $(LIBSRCP)

$(EXEM): $(SRCM) $(LIBSRCP) $(HEADERP) $(LIBSRCM) $(HEADERM)
	$(CC) -o $(EXEM) $(SRCM) $(LIBSRCP) $(LIBSRCM)

clean: 
	-rm -f $(EXE)
