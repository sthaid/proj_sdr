TARGETS=ut

LIBS=-lm -lsndfile -lpng -lportaudio -lfftw3 -lSDL2 -lSDL2_ttf -lrtlsdr
INCLUDE=-I../utils -I/usr/include/SDL2
UTILS=../utils/fft.c ../utils/misc.c ../utils/png_rw.c ../utils/sdl.c ../utils/wav.c ../utils/pa.c ../utils/filter.c

all: $(TARGETS)

ut: ut.c ut_plot_test.c ut_filter_test.c ut_ssb_test.c ut_antenna_test.c ut_rx_test.c sdr2.c $(UTILS)
	gcc -g -O2 -Wall $(INCLUDE) -o $@ $^ $(LIBS)

run: ut
	#./ut plot
	#./ut antenna
	#./ut filter
	#./ut rx_sim
	./ut rx_sdr
