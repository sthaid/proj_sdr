
TARGETS=sim

all: $(TARGETS)

sim: sim.c common.c util/util_sdl.c
	gcc -g -O2 -Wall -Iutil -I/usr/include/SDL2 -o $@ $^ -lsndfile -lm -lfftw3   -lSDL2 -lSDL2_ttf

run: sim
	./sim | ./spkr

src: src.c common.c common.h
	gcc -g -O2 -Wall -o src src.c common.c -lsndfile -lm

am_mod: am_mod.c common.c common.h
	gcc -g -O2 -Wall -o am_mod am_mod.c common.c -lsndfile -lm

clean:
	rm -f $(TARGETS)
