//#include <stdio.h>
//#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
//#include <unistd.h>
//#include <string.h>
//#include <errno.h>
//#include <math.h>
//#include <time.h>
//#include <pthread.h>
//#include <complex.h>
//#include <fftw3.h>

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>

#define TWO_PI (2*M_PI)

// -----------------  LOGGING  --------------------

#define NOTICE(fmt, args...) do { log_msg("NOTICE", fmt, ## args); } while (0)
#define WARN(fmt, args...)   do { log_msg("WARN", fmt, ## args); } while (0)
#define ERROR(fmt, args...)  do { log_msg("ERROR", fmt, ## args); } while (0)
#define FATAL(fmt, args...)  do { log_msg("FATAL", fmt, ## args); exit(1); } while (0)
#define DEBUG(fmt, args...)  do { if (debug) log_msg("FATAL", fmt, ## args); } while (0)

extern bool debug;
extern char *progname;

void log_msg(char *lvl, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

// -----------------  TIME  -----------------------

unsigned long microsec_timer(void);
unsigned long get_real_time_us(void);

#if 0
// -----------------  FAST SINE WAVE  -------------

void init_sine_wave(void);
double sine_wave(double f, double t);
#endif

// -----------------  DATA SET OPS  ---------------

double moving_avg(double v, int n, void **cx_arg);
void average_float(float *v, int n, double *min_arg, double *max_arg, double *avg);
void average(double *v, int n, double *min_arg, double *max_arg, double *avg);
void normalize(double *v, int n, double min, double max);

