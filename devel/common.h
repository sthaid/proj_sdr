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

#include <misc.h>
#include <sdl.h>
#include <png_rw.h>
#include <wav.h>
#include <fft.h>
#include <pa.h>
#include <filter.h>

#define MHZ 1000000
#define KHZ 1000

#define SAMPLE_RATE 2400000   // 2.4 MS/sec
#define DELTA_T     (1. / SAMPLE_RATE)

#define AUDIO_SAMPLE_RATE 22000

#define LPF_ORDER 20

#define ANTENNA_FILENAME "antenna.dat"

#define CTRL SDL_EVENT_KEY_CTRL
#define ALT  SDL_EVENT_KEY_ALT
#define NOC SDL_PLOT_NO_CURSOR

#define ATTRIB_UNUSED __attribute__ ((unused))

// ut.c
#define MAX_CTRL 14
#define MAX_CTRL_ENUM_NAMES 10
typedef struct {
    char name[100];
    char info[100];
    struct test_ctrl_s {
        char *name;
        double *val, min, max, step;
        char *val_enum_names[MAX_CTRL_ENUM_NAMES];
        char *units;
        int decr_event;
        int incr_event;
    } ctrl[MAX_CTRL];
} test_ctrl_t;

extern test_ctrl_t tc;
extern char       *test_name;

void audio_out(double yo);

void plot_clear(int idx);
void plot_real(int idx,
               double *data, int n,
               double xvmin, double xvmax, double yvmin, double yvmax,
               double xv_blue_cursor, double xv_red_cursor, char *title, char *x_units,
               int x_pos, int y_pos, int x_width, int y_height);
void plot_fft(int idx,
              complex *fft, int n, double sample_rate,
              double xv_min, double xv_max, double yv_max, double xv_blue_cursor, double xv_red_cursor, char *title,
              int x_pos, int y_pos, int x_width, int y_height);

// xxx
void *plot_test(void *cx);
void *filter_test(void *cx);
void *ssb_test(void *cx);
void *antenna_test(void *cx);
void *rx_test(void *cx);

// sdr2.c
void sdr_list_devices(void);
void sdr_init(double f, void(*cb)(unsigned char *iq, size_t len));
void sdr_set_freq(double f);

