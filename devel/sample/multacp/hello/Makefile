
DEV = IB
DEV = UDP

ACPDIR = $(HOME)/local/macp
MACPCC = macpcc -ndev $(DEV)
ACPCC  = acpcc  -ndev $(DEV)

OBJS_MULTMPI = multmpi.o

MPIACP_SRC = hello_mpi.c
MPIACP_EXE = hello_mpi_$(DEV)

ACP_SRC = hello_acp.c
ACP_EXE = hello_acp_$(DEV)

all: $(MPIACP_EXE) $(ACP_EXE)

$(MPIACP_EXE): $(MPIACP_SRC) $(OBJS_MULTMPI)
	$(MACPCC) -o $(MPIACP_EXE) $(MPIACP_SRC) $(OBJS_MULTMPI)

$(ACP_EXE): $(ACP_SRC) $(OBJS_MULTMPI)
	$(ACPCC) -o $(ACP_EXE) $(ACP_SRC)

multmpi.o: multmpi.c multmpi.h
	$(MACPCC) -I. -c multmpi.c

clean: 
	-rm -f $(MPIACP_EXE) $(ACP_EXE) *.o

clean_files:
	-rm core.*
	-rm Nodefile.*
	-rm Portfile.*
