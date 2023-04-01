
TARGETS=sdr

all: $(TARGETS)

sdr: sdr.c common.c common.h
	gcc -g -O2 -Wall -o sdr sdr.c common.c -lsndfile -lm -lrtlsdr

run: sdr
	./sdr

clean:
	rm -f $(TARGETS)
