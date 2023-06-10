LIBS=-lm -lportaudio -lfftw3 -lSDL2 -lSDL2_ttf -lrtlsdr
INCLUDE=-I/usr/include/SDL2
FLAGS=-g -O2 -Wall

sdr: main.c
	gcc $(FLAGS) $(INCLUDE) -o $@ $^ $(LIBS)

