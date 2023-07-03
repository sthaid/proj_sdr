#define _GNU_SOURCE

//xxx are all these needed
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

//
// defines
//

#ifdef MAIN
  #define EXTERN
#else
  #define EXTERN extern
#endif

#define KHZ 1000
#define MHZ 1000000

#define SDR_SAMPLE_RATE   2400000   // 2.4 MS/sec
#define AUDIO_SAMPLE_RATE 22000

#define MODE_FFT  0
#define MODE_SCAN 1
#define MODE_PLAY 2

#define MODE_STR(m) \
    ((m) == MODE_FFT  ? "FFT"  : \
     (m) == MODE_SCAN ? "SCAN" : \
     (m) == MODE_PLAY ? "PLAY" : \
                        "????")

#define MAX_BAND         20
#define MAX_STATION      20
#define MAX_SCAN_STATION 100
#define MAX_WATERFALL    500

#define MAX_SDR_ASYNC_RB_DATA  (16*32*512/2 * 2)  // 131072 * 2 = 262144

//
// typedefs
//

typedef long freq_t;

typedef struct {
    unsigned long head;
    unsigned long tail;
    complex data[MAX_SDR_ASYNC_RB_DATA];
} sdr_async_rb_t;

typedef struct band_s {
    // static config
    char *name;
    freq_t f_min;
    freq_t f_max;
    freq_t f_span;
    freq_t f_step;
    int max_station;
    struct station_s {
        freq_t f;
        char *name;
        // xxx demod
    } station[MAX_STATION];

    // state
    int  idx;
    bool selected;
    bool active;
//  bool play_inprog;
//  bool fft_inprog;
    freq_t f_fft_inprog_min;  // xxx may not need these 2
    freq_t f_fft_inprog_max;
    freq_t f_play;

    // fft buffers
    complex *fft_in;
    complex *fft_out;

    // fft result for entire band
    int      max_cabs_fft;
    double  *cabs_fft;

    // waterfall 
    struct wf_s {
        unsigned char *data;
        int            num;
        int            last_displayed_num;
        int            last_displayed_width;
        unsigned char *pixels8;
        texture_t      texture;
    } wf;

    // scan for stations
    int max_scan_station;
    struct scan_station_s {
        freq_t f;
        freq_t bw;
    } scan_station[MAX_SCAN_STATION];
} band_t;

//
// variables
//

EXTERN int     max_band;
EXTERN band_t *band[MAX_BAND];

EXTERN int     mode;
EXTERN bool    program_terminating;

//
// inline procedures
//

// set row=-1 is for a new entry
static inline unsigned char * get_waterfall(band_t *b, int row)
{
    int tmp = (b->wf.num - 1 - row);
    if (tmp < 0) {
        return NULL;
    }
    return b->wf.data + ((tmp % MAX_WATERFALL) * b->max_cabs_fft);
}

//
// prototypes
//

// radio.c
void radio_init(void);
bool radio_event(sdl_event_t *ev);

// config.c
void config_init(void);

// display.c
void display_init(void);
void display_handler(void);
void update_display_title_line(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

// audio.c
void audio_init(void);
void audio_out(double yo);

// sdr.c xxx check these
void sdr_init(int dev_idx, int sample_rate);
void sdr_list_devices(void);
void sdr_hardware_test(void);
void sdr_set_ctr_freq(freq_t f, bool sim);
void sdr_read_sync(complex *buff, int n, bool sim);
void sdr_read_async(sdr_async_rb_t *rb, bool sim);
void sdr_cancel_async(bool sim);

