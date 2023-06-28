TARGETS = sdl_test

LIBS    = -lSDL2 -lSDL2_ttf -lpng -lm
INCLUDE = -I../utils -I/usr/include/SDL2
FLAGS   = -g -O2 -Wall

UTILS   = ../utils/misc.c ../utils/sdl.c ../utils/png_rw.c

all: $(TARGETS)

sdl_test: sdl_test.c $(UTILS)
	gcc $(FLAGS) $(INCLUDE) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGETS)
