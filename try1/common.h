//xxx are all these needed
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <locale.h>
#include <string.h>
#include <complex.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <misc.h>
#include <sdl.h>
#include <pa.h>
#include <fft.h>
#include <filter.h>

#ifdef MAIN
  #define EXTERN
#else
  #define EXTERN extern
#endif

#define TWO_PI (2*M_PI)

#define SDR_SAMPLE_RATE   2400000   // 2.4 MS/sec
#define AUDIO_SAMPLE_RATE 22000

#define KHZ 1000
#define MHZ 1000000

// -----------------  xxxxxxxxxxx  ---------------------------------

typedef unsigned long freq_t;

#define MAX_SDR_ASYNC_RB_DATA  (16*32*512/2 * 2)  // 131072 * 2 = 262144

typedef struct {
    unsigned long head;
    unsigned long tail;
    complex data[MAX_SDR_ASYNC_RB_DATA];
} sdr_async_rb_t;

// -----------------  STRUCT BAND  ---------------------------------

#define MAX_BAND         20
#define MAX_STATION      20
#define MAX_SCAN_STATION 100
#define MAX_WATERFALL    500

typedef struct band_s {
    // static config
    char *name;
    freq_t f_min;
    freq_t f_max;
    freq_t f_step;
    int max_station;
    struct station_s {
        freq_t f;
        char *name;
        // xxx demod
    } station[MAX_STATION];

    // dynamic config
    freq_t f;
    int demod;
    int squelch;
    int selected;
    int active;

    // xxx clean up what follows
    int      num_fft;
    freq_t   fft_freq_span;
    double  *cabs_fft;
    int      max_cabs_fft;
    complex *fft_in;
    complex *fft_out;

    struct wf_s {
        unsigned char *data;
        int            num;
        int            last_displayed_num;
        int            last_displayed_width;
        unsigned char *pixels8;
        texture_t      texture;
    } wf;

    freq_t f_play;
    int max_scan_station;
    struct scan_station_s {
        freq_t f;
        freq_t bw;
    } scan_station[MAX_SCAN_STATION];
} band_t;

// use -1 is for a new entry
static inline unsigned char * get_waterfall(band_t *b, int row)
{
    int tmp = (b->wf.num - 1 - row);
    if (tmp < 0) {
        return NULL;
    }
    return b->wf.data + ((tmp % MAX_WATERFALL) * b->max_cabs_fft);
}

EXTERN band_t *band[MAX_BAND];
EXTERN int     max_band;

// -----------------  VARIABLES  -----------------------------------

EXTERN bool program_terminating;
EXTERN int  play_time; //xxx del

#if 0
// xxx not used yet
EXTERN int           zoom;
EXTERN int           volume;
EXTERN int           mute;
EXTERN int           scan_intvl;
EXTERN int           help;
#endif

// -----------------  PROTOTYPES  ----------------------------------

// radio.c
void radio_init(void);

// config.c
void config_init(void);

// display.c
void display_init(void);
void display_handler(void);

// audio.c
void audio_init(void);
void audio_out(double yo);

// sdr.c xxx check these
void sdr_init(int dev_idx, int sample_rate);
void sdr_list_devices(void);
void sdr_hardware_test(void);
void sdr_read_sync(freq_t ctr_freq, complex *buff, int n);
void sdr_read_async(freq_t ctr_freq, sdr_async_rb_t *rb);
void sdr_cancel_async(void);

