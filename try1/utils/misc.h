#ifndef __MISC_H__
#define __MISC_H__

#include <stdarg.h>
#include <complex.h>
#include <wchar.h>

#define ATTRIB_UNUSED __attribute__ ((unused))

#define TWO_PI (2*M_PI)

#define int_to_str(v) ({static char s[20]; sprintf(s, "%d", v); s;})

// -----------------  LOGGING  --------------------

#define NOTICE(fmt, args...) do { log_msg(__FILE__, __LINE__, "NOTICE", fmt, ## args); } while (0)
#define WARN(fmt, args...)   do { log_msg(__FILE__, __LINE__, "WARN", fmt, ## args); } while (0)
#define ERROR(fmt, args...)  do { log_msg(__FILE__, __LINE__, "ERROR", fmt, ## args); } while (0)
#define DEBUG(fmt, args...)  do { if (0) log_msg(__FILE__, __LINE__, "DEBUG", fmt, ## args); } while (0)
#define FATAL(fmt, args...)  do { log_msg(__FILE__, __LINE__, "FATAL", fmt, ## args); } while (0)

#define BLANKLINE do { log_msg(__FILE__, __LINE__, "", "\n"); } while (0)

#define ASSERT_MSG(cond, fmt, args...) \
    do { \
        if (cond) break; \
        char msg[100]; \
        snprintf(msg, sizeof(msg), fmt, ## args); \
        FATAL("ASSERTION FAILED: %s: %s\n", #cond, msg); \
    } while (0)

#define ASSERT(cond) \
    do { \
        if (cond) break; \
        FATAL("ASSERTION FAILED: %s\n", #cond); \
    } while (0)

void log_msg(char *file, int line, char *lvl, char *fmt, ...) __attribute__ ((format (printf, 4, 5)));

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
unsigned int round_up(unsigned int n, unsigned int multiple);

#endif
