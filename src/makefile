CC = gcc
RM = rm -f

BIN = netsukuku

OBJ = accept.o gmap.o hook.o if.o inet.o ipv6-gmp.o krnl_route.o krnl_rule.o libnetlink.o\
      ll_map.o llist.o log.o map.o netsukuku.o pkts.o qradar.o request.o route.o xmalloc.o 

CFLAGS = -ggdb

$(BIN): $(OBJ)
	$(CC) $^ -o $(BIN)
	
netsukuku:  $(OBJ)
	$(LD) -o $@ $(OBJ)

qspn: xmalloc.o qspn-empiric.o map.o ipv6-gmp.o log.o xmalloc.o
	$(CC)  $(CFLAGS) $^ -o qspn-empiric -lgmp -lpthread	
	
clean: $(OBJ) $(ECT) $(BIN)
	$(RM) $^ 
	$(RM) $(LOG)
