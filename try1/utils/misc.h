#ifndef __MISC_H__
#define __MISC_H__

#include <stdarg.h>
#include <complex.h>
#include <wchar.h>

#define ATTRIB_UNUSED __attribute__ ((unused))

#define TWO_PI (2*M_PI)

// -----------------  LOGGING  --------------------

#define NOTICE(fmt, args...) do { log_msg("NOTICE", fmt, ## args); } while (0)
#define WARN(fmt, args...)   do { log_msg("WARN", fmt, ## args); } while (0)
#define ERROR(fmt, args...)  do { log_msg("ERROR", fmt, ## args); } while (0)
#define DEBUG(fmt, args...)  do { if (0) log_msg("FATAL", fmt, ## args); } while (0)

#define FATAL(fmt, args...)  do { log_msg("FATAL", fmt, ## args); log_msg("FATAL", "%s %d\n", __FILE__, __LINE__); exit(1); } while (0)

#define BLANKLINE do { log_msg("", "\n"); } while (0)

void log_msg(char *lvl, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

// -----------------  TIME  -----------------------

unsigned long microsec_timer(void);
unsigned long get_real_time_us(void);

// -----------------  DATA SET OPS  ---------------

double moving_avg(double v, int n, void **cx_arg);
void average_float(float *v, int n, double *min_arg, double *max_arg, double *avg);
void average(double *v, int n, double *min_arg, double *max_arg, double *avg);
void normalize(double *v, int n, double min, double max);

// -----------------  STRINGS  -------------------------

void remove_trailing_newline(char *s);
void remove_leading_whitespace(char *s);
int mbstrchars(char *s);

// -----------------  MISC  ----------------------------

void zero_real(double *data, int n);
void zero_complex(complex *data, int n);

#endif
