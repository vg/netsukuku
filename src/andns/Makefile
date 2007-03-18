LIBS = -lcrypto -lz
OBJ = andns_net.o andns_lib.o andns_shared.o 
NAME = libandns.so
OPTS = -fPIC -Wall 
CC = gcc $(OPTS)
LIB_DIR=/usr/lib/

VERSION=`grep ANDNS_SHARED_VERSION andns_shared.h | cut -d'"' -f2`
PREFIX_VERSION=`grep ANDNS_SHARED_VERSION andns_shared.h | cut -d'"' -f2 | cut -d'.' -f1`

E = @echo
Q = @

all: $(OBJ)
	$(E) "  LD      " $(NAME)${VERSION}
	$(Q) gcc -shared -Wl,-soname,${NAME}.${PREFIX_VERSION} -o ${NAME}.${VERSION} ${OBJ}

%.o: %.c
	$(E) "  CC      " $@
	$(Q) $(CC) -c $(CFLAGS) $< -o $@

install: 
	$(E) "  I      " $(NAME).${VERSION}
	$(Q) if [ -f ${LIB_DIR}${NAME}.${PREFIX_VERSION} ]; then rm ${LIB_DIR}${NAME}.${PREFIX_VERSION}; fi
	$(Q) if [ -f ${LIB_DIR}${NAME} ]; then rm ${LIB_DIR}${NAME}; fi
	$(Q) install $(NAME).${VERSION} ${LIB_DIR}
	$(Q) ln -s $(NAME).${VERSION} ${LIB_DIR}${NAME}.${PREFIX_VERSION}
	$(Q) ln -s $(NAME).${VERSION} ${LIB_DIR}${NAME}

.PHONY: clean
clean:
	rm -rf $(OBJ) $(NAME) $(NAME).${VERSION}
