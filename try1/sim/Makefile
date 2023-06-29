TARGETS = sim

LIBS    = -lm -lfftw3 -lsndfile
INCLUDE = -I../utils
FLAGS   = -g -O2 -Wall

UTILS   = ../utils/misc.c ../utils/fft.c ../utils/filter.c ../utils/wav.c

all: $(TARGETS)

sim: sim.c $(UTILS)
	gcc $(FLAGS) $(INCLUDE) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGETS)
