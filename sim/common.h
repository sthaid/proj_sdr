#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#define TWO_PI (2*M_PI)

// -----------------  LOGGING  --------------------

#define NOTICE(fmt, args...) log_msg("NOTICE", fmt, ## args);
#define WARN(fmt, args...) log_msg("WARN", fmt, ## args);
#define ERROR(fmt, args...) log_msg("ERROR", fmt, ## args);

extern char *progname;

void log_msg(char *lvl, char *fmt, ...);

// -----------------  TIME  -----------------------

unsigned long microsec_timer(void);
unsigned long get_real_time_us(void);

// -----------------  AUDIO SRC  ------------------

double get_src(int id, double t);
void init_audio_src(void);

// -----------------  FILTERS  --------------------

double lpf(double x);

// -----------------  SINE WAVE  ------------------

double sine_wave(double f, double t);
