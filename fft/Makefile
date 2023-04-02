filters: filters.c pa_utils.c sf_utils.c util/util_sdl.c util/util_misc.c util/util_jpeg.c util/util_png.c
	gcc -g -Wall -O2 -I. -Iutil -I/usr/include/SDL2 $^ \
            -lSDL2 -lSDL2_ttf -ljpeg -lpng -lfftw3 -lsndfile -lportaudio -lpthread -lm \
            -o $@ 

