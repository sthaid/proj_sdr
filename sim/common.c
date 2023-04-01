#include "common.h"

// -----------------  LOGGING  --------------------------------------------

void log_msg(char *lvl, char *fmt, ...)
{
    char s[200];
    va_list ap;
    int len;

    va_start(ap, fmt);
    vsnprintf(s, sizeof(s), fmt, ap);
    va_end(ap);

    len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }

    fprintf(stderr, "%s %s: %s\n", lvl, progname, s);
}

// -----------------  TIME  -----------------------------------------------

unsigned long microsec_timer(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((unsigned long)ts.tv_sec * 1000000) + ((unsigned long)ts.tv_nsec / 1000);
}

unsigned long get_real_time_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME,&ts);
    return ((unsigned long)ts.tv_sec * 1000000) + ((unsigned long)ts.tv_nsec / 1000);
}

// -----------------  AUDIO SRC  ------------------

// reference:
//   apt install libsndfile1-dev
//   file:///usr/share/doc/libsndfile1-dev/html/api.html
//   https://github.com/libsndfile/libsndfile

#include <sndfile.h>

// defines

#define MAX_SRC 10

// variables

static struct src_s {
    int max;
    int sample_rate;
    float *data;
} src[MAX_SRC];

// prototypes

static void init_audio_src_from_wav_file(int id, char *filename);
static int read_wav_file(char *filename, float **data, int *num_chan, int *num_items, int *sample_rate);

double get_src(int id, double t)
{
    int idx;
    struct src_s *s = &src[id];

    idx = (unsigned long)nearbyint(t * s->sample_rate) % s->max;
    //printf("get t=%f %d %f\n", t, idx, s->data[idx]);
    return s->data[idx];
}

void init_audio_src(void)  // xxx caller should pass in list of srcs to init
{
    //init_audio_src_from_wav_file(0, "super_critical.wav");
    init_audio_src_from_wav_file(0, "one_bourbon_one_scotch_one_beer.wav");
    init_audio_src_from_wav_file(1, "proud_mary.wav");
}

static void init_audio_src_from_wav_file(int id, char *filename)
{
    int    rc, num_chan, num_items, sample_rate;
    struct src_s *s = &src[id];
    float *data;

    rc = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (rc != 0) {
        ERROR("read_wav_file %s, %s\n", filename, strerror(errno));
        exit(1);
    }
    NOTICE("num_chan=%d num_items=%d sample_rate=%d\n", num_chan, num_items, sample_rate);

    if (num_chan != 1) {
        ERROR("num_chan must be 1\n", num_chan);
        exit(1);
    }

    float max = 0;
    float min = 0;
    for (int i = 0; i < num_items; i++) {
        if (data[i] > max) {
            max = data[i];
        }
        if (data[i] < min) {
            min = data[i];
        }
    }
    //printf("min, max %f %f\n", min, max);
    //exit(1);

    for (int i = 0; i < num_items; i++) {
        data[i] *= 1.4;
    }

    s->max         = num_items;
    s->sample_rate = sample_rate;
    s->data        = data;
}

static int read_wav_file(char *filename, float **data, int *num_chan, int *num_items, int *sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt, items;
    float   *d;

    // preset return values
    *data = NULL;
    *num_chan = 0;
    *num_items = 0; 
    *sample_rate = 0;

    // open wav file and get info
    memset(&sfinfo, 0, sizeof (sfinfo));
    file = sf_open(filename, SFM_READ, &sfinfo);
    if (file == NULL) {
        return -1;
    }

    // allocate memory for the data
    items = sfinfo.frames * sfinfo.channels;
    d = malloc(items*sizeof(float));

    // read the wav file data 
    cnt = sf_read_float(file, d, items);
    if (cnt != items) {
        free(d);
        sf_close(file);
        return -1;
    }

    // close file
    sf_close(file);

    // return values
    *data        = d;
    *num_chan    = sfinfo.channels;
    *num_items   = items;
    *sample_rate = sfinfo.samplerate;
    return 0;
}

// -----------------  FILTERS  --------------------

#if 1
static inline double low_pass_filter(double v, double *cx, double k2)
{
    *cx = k2 * *cx + (1-k2) * v;
    return *cx;
}

static inline double low_pass_filter_ex(double v, double *cx, int k1, double k2)
{
    for (int i = 0; i < k1; i++) {
        v = low_pass_filter(v, &cx[i], k2);
    }
    return v;
}

double lpf(double v)
{
    static double cx[20];
    return low_pass_filter_ex(v, cx, 2, .90);
}
#else
double lpf(double v)
{
    static double cx;
    static double k2 = 0.85;

    cx = k2 * cx + (1-k2) * v;
    return cx;
}
#endif

// -----------------  SINE WAVE  ------------------

static double sine[1000];

void init_sine_wave(void)
{
    for (int i = 0; i < 1000; i++) {
        sine[i] = sin((TWO_PI / 1000) * i);
    }
}

double sine_wave(double f, double t)
{
    double iptr;
    int idx;

    idx = modf(f*t, &iptr) * 1000;
    if (idx < 0 || idx >= 1000) {
        ERROR("sine_wave idx %d\n", idx); //xxx del
        exit(1);
    }
    return sine[idx];
}

