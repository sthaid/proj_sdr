TARGETS=gtf

all: $(TARGETS)

gtf: gtf.c ../utils/sdl.c ../utils/misc.c ../utils/png_rw.c ../utils/wav.c
	gcc -g -O2 -Wall -I../utils -I/usr/include/SDL2 -o $@ $^ -lsndfile -lpng -lm -lfftw3 -lSDL2 -lSDL2_ttf

