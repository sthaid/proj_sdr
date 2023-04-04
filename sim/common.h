#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TWO_PI (2*M_PI)

// -----------------  LOGGING  --------------------

#define NOTICE(fmt, args...) log_msg("NOTICE", fmt, ## args);
#define WARN(fmt, args...) log_msg("WARN", fmt, ## args);
#define ERROR(fmt, args...) log_msg("ERROR", fmt, ## args);

extern char *progname;

void log_msg(char *lvl, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

// -----------------  TIME  -----------------------

unsigned long microsec_timer(void);
unsigned long get_real_time_us(void);

// -----------------  AUDIO SRC  ------------------

double get_src(int id, double t);
void init_audio_src(void);

// -----------------  FILTERS  --------------------

double lpf(double x, int k1, double k2);

// -----------------  SINE WAVE  ------------------

void init_sine_wave(void);
double sine_wave(double f, double t);

// -----------------  xxxxxxxxx  ------------------

double moving_avg(double v, int n, void **cx_arg);
void average_float(float *v, int n, double *min_arg, double *max_arg, double *avg);

