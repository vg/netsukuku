LIBS = -lcrypto -lz
OBJ = andns_net.o andns_lib.o andns_shared.o 
NAME = 
OPTS = -g -Wall #-O3 -fexpensive-optimizations -fomit-frame-pointer -Wall
CCFLAGS =
CC = gcc $(CCFLAGS) $(OPTS)

E = @echo
Q = @

all: $(OBJ)
	$(E) "  LD      " $(NAME)
	$(Q) $(CC) -o $(NAME) $(OBJ) $(LIBS)

%.o: %.c
	$(E) "  CC      " $@
	$(Q) $(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ) $(NAME) *~ .*swp
