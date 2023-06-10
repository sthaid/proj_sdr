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

#define MAX_BAND    20
#define MAX_STATION 20

typedef struct {
    char *name;
    double f_min;
    double f_max;
    double f_curr;
    double f_step;
    double f_round;
    double squelch;
    int    flags;
    struct {
        char *name;
        double f;
    } station[MAX_STATION];
} band_t;

band_t band[MAX_BAND];

void config_init(void);
void sdr_init(void);
void audio_init(void);
void fft_init(void);
void display_init(void);
