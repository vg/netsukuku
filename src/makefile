CC = gcc
RM = rm -f

BIN = netsukuku

OBJ = accept.o llist.o log.o map.o misc.o request.o xmalloc.o bmap.o daemon.o \
      pkts.o ll_map.o libnetlink.o  radar.o gmap.o hook.o ipv6-gmp.o if.o inet.o \
      krnl_route.o krnl_rule.o qspn.o route.o tracer.o netsukuku.o 

LIBS = -lgmp -lpthread

CFLAGS = -ggdb


#-Wall

$(BIN): $(OBJ)
	$(CC) $^ -o $(BIN)
	
netsukuku:  $(OBJ)
	$(CC)  $(CFLAGS) $^ -o netsukuku_d -lgmp -lpthread	
	

qspn: xmalloc.o qspn-empiric.o map.o ipv6-gmp.o log.o xmalloc.o misc.o
	$(CC)  $(CFLAGS) $^ -o qspn-empiric -lgmp -lpthread	
	
clean:
	$(RM) $(OBJ) netsukuku_d

clean-qspn: xmalloc.o qspn-empiric.o map.o ipv6-gmp.o log.o xmalloc.o qspn-empiric
	$(RM) $^

config-os:
	host_os=`uname -s`; \
	echo "Compiling on $${host_os} " ; \
	if [ `echo $${host_os} | grep -i BSD` ]; then \
		if [ ! -f .os_configured ]; then \
			cp includes.h .includes.h-old; \
			echo "#define BSD" >> includes.h; \
			touch .os_configured; \
		fi \
	fi
