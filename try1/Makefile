LIBS    = -lm -lportaudio -lfftw3 -lSDL2 -lSDL2_ttf -lrtlsdr -lpng
INCLUDE = -I./utils -I/usr/include/SDL2
FLAGS   = -g -O2 -Wall

UTILS = utils/misc.c utils/sdl.c utils/png_rw.c utils/pa.c utils/fft.c utils/filter.c

sdr: main.c config.c display.c audio.c radio.c sdr.c $(UTILS)
	gcc $(FLAGS) $(INCLUDE) -o $@ $^ $(LIBS)

clean:
	rm -f sdr
