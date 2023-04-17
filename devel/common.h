#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
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

// ut_antenna.c
void init_antenna(void);
double get_antenna(double t, double f_center);

