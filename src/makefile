CC = gcc
RM = rm -f

BIN = netsukuku

OBJ = accept.o gmap.o hook.o if.o inet.o ipv6-gmp.o krnl_route.o krnl_rule.o libnetlink.o\
      ll_map.o llist.o log.o map.o netsukuku.o pkts.o qradar.o request.o route.o xmalloc.o 

$(BIN): $(OBJ)
	$(CC) $^ -o $(BIN)
	
netsukuku:  $(OBJ)
	$(LD) -o $@ $(OBJ)

#qspn: log.o xmalloc.o ipv6-gmp.o inet.o map.o gmap.o qspn-empiric.o pkts.o
qspn: xmalloc.o qspn-empiric.o
	$(CC) $^ -o qspn-empiric
	$(LD) -o qspn-empiric $^

	
clean: $(OBJ) $(ECT) $(BIN)
	$(RM) $^ 
	$(RM) $(LOG)
