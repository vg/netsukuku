CC = gcc
RM = rm -f

BIN = netsukuku

OBJ = accept.o arp.o if.o inet.o krnl_route.o krnl_rule.o libnetlink.o ll_map.o llist.o log.o map.o netsukuku.o pkts.o radar.o request.o route.o xmalloc.o

#ipv6-gmp.c

$(BIN): $(OBJ)
	$(CC) $^ -o $(BIN)
	
netsukuku:  $(OBJ)
	$(LD) -o $@ $(OBJ)

qspn-empiric: qspn-empiric.o map.o inet.o ipv6-gmp.o gmap.o xmalloc.o log.o
#	$(CC) $^ -o 
	$(CC) -g -c xmalloc.o xmalloc.c
	$(CC) -g -c log.o log.c
	$(CC) -g -lgmp -c ipv6-gmp.o ipv6-gmp.c
	$(CC) -g -c inet.o inet.c
	$(CC) -g -c gmap.o gmap.c
	$(CC) -g -c map.o map.c
	$(CC) -g -c qspn-empiric.o qspn-empiric.c
	echo "qspn-empiric"
	$(LD) -o qspn-empiric $@

	
clean: $(OBJ) $(ECT) $(BIN)
	$(RM) $^ 
	$(RM) $(LOG)
