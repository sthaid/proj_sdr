all: t1 filters

t1: t1.c common.c common.h
	gcc -o t1 -Wall t1.c common.c -lsndfile -lm

filters: filters.c pa_utils.c sf_utils.c util/util_sdl.c util/util_misc.c util/util_jpeg.c util/util_png.c
	gcc -g -Wall -O2 -I. -Iutil -I/usr/include/SDL2 $^ \
            -lSDL2 -lSDL2_ttf -ljpeg -lpng -lfftw3 -lsndfile -lportaudio -lpthread -lm \
            -o $@ 

