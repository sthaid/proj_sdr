
TARGETS=src

all: $(TARGETS)

src: src.c common.c common.h
	gcc -g -O2 -Wall -o src src.c common.c -lsndfile

clean:
	rm -f $(TARGETS)
