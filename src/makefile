CC = gcc
RM = rm -f

BIN = netsukuku

OBJ = accept.o bmap.o daemon.o gmap.o hook.o if.o inet.o ipv6-gmp.o krnl_route.o\
      krnl_rule.o libnetlink.o ll_map.o llist.o log.o map.o misc.o netsukuku.o  \
      pkts.o qspn.o radar.o request.o route.o tracer.o xmalloc.o

LIBS = -lgmp -lpthread

CFLAGS = -ggdb

$(BIN): $(OBJ)
	$(CC) $^ -o $(BIN)
	
netsukuku:  $(OBJ)
	$(LD) -o $@ $(OBJ) $(LIBS)

qspn: xmalloc.o qspn-empiric.o map.o ipv6-gmp.o log.o xmalloc.o misc.o
	$(CC)  $(CFLAGS) $^ -o qspn-empiric -lgmp -lpthread	
	
clean: $(OBJ) $(ECT) $(BIN)
	$(RM) $^ 
	$(RM) $(LOG)

clean-qspn: xmalloc.o qspn-empiric.o map.o ipv6-gmp.o log.o xmalloc.o qspn-empiric
	$(RM) $^
