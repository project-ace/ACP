include ../../../Makefile.inc

OBJS = acpbl.o

ALL: $(LIB_VER_REAL)

$(LIB_VER_REAL): $(OBJS) acpbl.o
	#$(CC) -shared -Wl,-soname,$(LIB_VER_REAL) -o $(LIB_VER_REAL) $(OBJS)
	#ln -sf $(LIB_VER_REAL) $(LIB_REAL)

clean: 
	-rm -f $(SRCDIR)/*.o
	-rm -f $(LIB_VER_REAL) $(LIB_REAL)
