
TARGETS=sim

all: $(TARGETS)

sim: sim.c common.c common.h
	gcc -g -O2 -Wall -o sim sim.c common.c -lsndfile -lm

src: src.c common.c common.h
	gcc -g -O2 -Wall -o src src.c common.c -lsndfile -lm

am_mod: am_mod.c common.c common.h
	gcc -g -O2 -Wall -o am_mod am_mod.c common.c -lsndfile -lm

clean:
	rm -f $(TARGETS)
