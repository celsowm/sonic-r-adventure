CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
ROOT = ../..
GAME = $(ROOT)/source/sdl/src/adventure

GAME_SOURCES = $(GAME)/adventure_game.c $(GAME)/adventure_input.c \
               $(GAME)/adventure_math.c $(GAME)/adventure_player.c
GAME_HEADERS = $(GAME)/adventure_game.h $(GAME)/adventure_input.h \
               $(GAME)/adventure_math.h $(GAME)/adventure_player.h

.PHONY: test clean
test: test_adventure_game
	./test_adventure_game

test_adventure_game: test_adventure_game.c $(GAME_SOURCES) $(GAME_HEADERS)
	$(CC) $(CFLAGS) -I$(GAME) -o $@ test_adventure_game.c $(GAME_SOURCES) -lm

clean:
	rm -f test_adventure_game
