LIBS    = -lm -lportaudio -lfftw3 -lSDL2 -lSDL2_ttf -lrtlsdr
INCLUDE = -I./utils -I/usr/include/SDL2
FLAGS   = -g -O2 -Wall

UTILS = utils/misc.c

sdr: common.h $(UTILS:.c=.h)

sdr: main.c config.c display.c $(UTILS)
	gcc $(FLAGS) $(INCLUDE) -o $@ $^ $(LIBS)

clean:
	rm sdr
