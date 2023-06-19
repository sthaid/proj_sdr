//xxx are all these needed
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <locale.h>


#include <string.h>
//#include <errno.h>
#include <time.h>
#include <pthread.h>
//#include <ctype.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
#include <complex.h>
#include <math.h>

#include <misc.h>
#include <sdl.h>
#include <sdr.h>
#include <pa.h>

#ifdef MAIN
  #define EXTERN
#else
  #define EXTERN extern
#endif

#define TWO_PI (2*M_PI)

#define SDR_SAMPLE_RATE 2400000   // 2.4 MS/sec

// -----------------  CONFIG  ----------------------------

#define MAX_BAND    20
#define MAX_STATION 20

// xxx malloc these
struct band_s {
    // static config
    char *name;
    double f_min;
    double f_max;
    double f_step;
    int max_station;
    struct {
        double freq;
        char *name;
    } station[MAX_STATION];

    // dynamic config
    double f_curr;
    int demod;
    int squelch;
    int selected;
    int active;
};

EXTERN int           max_band;
EXTERN struct band_s band[MAX_BAND];

EXTERN int           zoom;
EXTERN int           volume;
EXTERN int           mute;
EXTERN int           scan_intvl;
EXTERN int           help;

EXTERN bool          program_terminating;

// -----------------  PROTOTYPES  ------------------------

// config.c
void config_init(void);
void config_write(void);

// display.c
void display_init(void);
void display_handler(void);

// audio.c
void audio_init(void);
void audio_out(double yo);

//void fft_init(void);
