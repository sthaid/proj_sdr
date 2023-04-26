TARGETS=ut

LIBS=-lm -lsndfile -lpng -lportaudio -lfftw3 -lSDL2 -lSDL2_ttf
INCLUDE=-I../utils -I/usr/include/SDL2
UTILS=../utils/fft.c ../utils/misc.c ../utils/png_rw.c ../utils/sdl.c ../utils/wav.c ../utils/pa.c ../utils/filter.c

all: $(TARGETS)

ut: ut.c ut_antenna.c $(UTILS)
	gcc -g -O2 -Wall $(INCLUDE) -o $@ $^ $(LIBS)

run: ut
	#./ut plot
	./ut antenna
	#./ut filter
	#./ut rx

#################

gen: gen.c $(UTILS)
	gcc -g -O2 -Wall $(INCLUDE) -o $@ $^ $(LIBS)

run_gen: gen
	./gen > out

old: gen.c ../utils/sdl.c ../utils/misc.c ../utils/png_rw.c ../utils/wav.c
	gcc -g -O2 -Wall -I../utils -I/usr/include/SDL2 -o $@ $^ -lsndfile -lpng -lm -lfftw3 -lSDL2 -lSDL2_ttf


#gcc -g -Wall -O2 -I. -Iutil -I/usr/include/SDL2 $^ 
#-lSDL2 -lSDL2_ttf -ljpeg -lpng -lfftw3 -lsndfile -lportaudio -lpthread -lm 
