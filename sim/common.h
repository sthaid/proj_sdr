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
#define SAMPLE_RATE 22000

// -----------------  LOGGING  --------------------

#define NOTICE(fmt, args...) log_msg("NOTICE", fmt, ## args);
#define WARN(fmt, args...) log_msg("WARN", fmt, ## args);
#define ERROR(fmt, args...) log_msg("ERROR", fmt, ## args);

extern char *progname;

void log_msg(char *lvl, char *fmt, ...);

// -----------------  TIME  -----------------------

unsigned long microsec_timer(void);
unsigned long get_real_time_us(void);

