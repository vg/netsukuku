BIN_DIR=/usr/bin/
all:
	gcc -Wall -landns ntkresolv.c -o ntkresolv
ntkresolv:
	gcc -Wall -landns ntkresolv.c -o ntkresolv
install: ntkresolv
	install -m 0755 ntkresolv ${BIN_DIR}

.PHONY: clean
clean:
	rm -f ntkresolv

