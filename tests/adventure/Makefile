CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
ROOT = ../..
GAME = $(ROOT)/source/sdl/src/adventure

.PHONY: test clean
test: test_adventure_game
	./test_adventure_game

test_adventure_game: test_adventure_game.c $(GAME)/adventure_game.c $(GAME)/adventure_game.h
	$(CC) $(CFLAGS) -I$(GAME) -o $@ test_adventure_game.c $(GAME)/adventure_game.c

clean:
	rm -f test_adventure_game
