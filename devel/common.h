#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <complex.h>
#include <math.h>

#include <misc.h>
#include <sdl.h>
#include <png_rw.h>
#include <wav.h>
#include <fft.h>
#include <pa.h>
#include <filter.h>

// ut_antenna.c
void init_antenna(void);
double get_antenna(double t);

// sdr2.c
void sdr_init(double f, void(*cb)(unsigned char *iq, size_t len));
void sdr_set_freq(double f);
