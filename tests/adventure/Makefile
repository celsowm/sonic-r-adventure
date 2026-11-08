CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
ROOT = ../..
GAME = $(ROOT)/source/sdl/src/adventure

GAME_SOURCES = $(GAME)/adventure_game.c $(GAME)/adventure_input.c \
               $(GAME)/adventure_math.c $(GAME)/adventure_player.c \
               $(GAME)/adventure_collision.c $(GAME)/adventure_vertical.c \
               $(GAME)/adventure_roof.c $(GAME)/adventure_capsule.c \
               $(GAME)/adventure_slide.c $(GAME)/adventure_roof_support.c \
               $(GAME)/adventure_solver.c $(GAME)/adventure_slope.c
GAME_HEADERS = $(GAME)/adventure_game.h $(GAME)/adventure_input.h \
               $(GAME)/adventure_math.h $(GAME)/adventure_player.h \
               $(GAME)/adventure_collision.h $(GAME)/adventure_terrain.h \
               $(GAME)/adventure_vertical.h $(GAME)/adventure_roof.h \
               $(GAME)/adventure_capsule.h $(GAME)/adventure_slide.h \
               $(GAME)/adventure_solver.h $(GAME)/adventure_slope.h

.PHONY: test clean
test: test_adventure_game
	./test_adventure_game

test_adventure_game: test_adventure_game.c $(GAME_SOURCES) $(GAME_HEADERS)
	$(CC) $(CFLAGS) -I$(GAME) -o $@ test_adventure_game.c $(GAME_SOURCES) -lm

clean:
	rm -f test_adventure_game
