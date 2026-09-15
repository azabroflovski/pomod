CFLAGS ?= -O2 -Wall -Wextra

pomod: pomod.c

clean:
	rm -f pomod

.PHONY: clean
