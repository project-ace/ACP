include ../../../Makefile.inc

OBJS_ACPBL = acpbl_test.o
OBJS_ACPBL_OHANDLE = acpbl_test_order_handle.o

EXE_ACPBL = acpbl
EXE_ACPBL_OHANDLE = acpbl_ohandle

all: $(EXE_ACPBL) $(EXE_ACPBL_OHANDLE)

acpbl_test_order_handle.o: acpbl_test_order_handle.c
	$(CC) $(CCFLAGS) -O3 -c acpbl_test_order_handle.c

$(EXE_ACPBL): $(OBJS_ACPBL)
	$(LD) $(LDFLAGS) -o $(EXE_ACPBL) $(OBJS_ACPBL) $(OBJ_ACPBL_PRIM)
$(EXE_ACPBL_OHANDLE): $(OBJS_ACPBL_OHANDLE)
	$(LD) $(LDFLAGS) -o $(EXE_ACPBL_OHANDLE) $(OBJS_ACPBL_OHANDLE) $(OBJ_ACPBL_PRIM)

clean: 
	-rm -f $(OBJS_ACPBL)
	-rm -f $(OBJS_ACPBL_OHANDLE)
	-rm -f $(EXE_ACPBL)
	-rm -f $(EXE_ACPBL_OHANDLE)
