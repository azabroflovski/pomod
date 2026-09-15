CFLAGS ?= -O2 -Wall -Wextra

# any memory bug or UB kills the process with a report instead of going unnoticed
SANITIZE = -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all

pomod: pomod.c

pomod-debug: pomod.c
	$(CC) $(CFLAGS) $(SANITIZE) -o $@ pomod.c

debug: pomod-debug

test: pomod pomod-debug
	./test.sh ./pomod
	./test.sh ./pomod-debug

clean:
	rm -rf pomod pomod-debug pomod-debug.dSYM

.PHONY: debug test clean
