CC = gcc
RM = rm -f

BIN = netsukuku

OBJ = accept.o accept.h arp.o if.o inet.o krnl_route.o krnl_rule.o libnetlink.o ll_map.o llist.o log.o map.o netsukuku.o pkts.o radar.o request.o route.o xmalloc.o

#ipv6-gmp.c

$(BIN): $(OBJ)
	$(CC) $^ -o $(BIN)
	
netsukuku:  $(OBJ)
	$(LD) -o $@ $(OBJ)


	
clean: $(OBJ) $(ECT) $(BIN)
	$(RM) $^ 
	$(RM) $(LOG)
