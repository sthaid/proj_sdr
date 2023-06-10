//xxx are all these needed
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

#ifdef MAIN
  #define EXTERN extern
#else
  #define EXTERN
#endif

// -----------------  LOGGING  ---------------------------

#define NOTICE(fmt, args...) do { log_msg("NOTICE", fmt, ## args); } while (0)
#define WARN(fmt, args...)   do { log_msg("WARN", fmt, ## args); } while (0)
#define ERROR(fmt, args...)  do { log_msg("ERROR", fmt, ## args); } while (0)
#define FATAL(fmt, args...)  do { log_msg("FATAL", fmt, ## args); log_msg("FATAL", "%s %d\n", __FILE__, __LINE__); exit(1); } while (0)
#define DEBUG(fmt, args...)  do { if (false) log_msg("DEBUG", fmt, ## args); } while (0)

EXTERN char *progname;

void log_msg(char *lvl, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

// -----------------  CONFIG  ----------------------------

#define MAX_BAND    20
#define MAX_STATION 20

struct band_s {
    char *name;
    double f_min;
    double f_max;
    double f_step;
    double f_curr;
    int demod;
    int squelch;
    int selected;
    int active;
    int max_station;
    struct {
        double freq;
        char *name;
    } station[MAX_STATION];
};


EXTERN int           max_band;
EXTERN struct band_s band[MAX_BAND];

EXTERN int           zoom;
EXTERN int           volume;
EXTERN int           mute;
EXTERN int           scan_intvl;
EXTERN int           help;


// -----------------  PROTOTYPES  ------------------------

void config_init(void);
void config_write(void);

void sdr_init(void);
void audio_init(void);
void fft_init(void);
void display_init(void);
