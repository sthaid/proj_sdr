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

static void init_audio_src_sine_wave(int id, int hz);
static void init_audio_src_white_noise(int id);
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
    init_audio_src_sine_wave(0, 300);
    init_audio_src_white_noise(1);
    init_audio_src_from_wav_file(2, "proud_mary.wav");
    init_audio_src_from_wav_file(3, "one_bourbon_one_scotch_one_beer.wav");
    init_audio_src_from_wav_file(4, "primitive_cool.wav");
    init_audio_src_from_wav_file(5, "super_critical.wav");
}

static void init_audio_src_sine_wave(int id, int hz)
{
    struct src_s *s = &src[id];

    s->max = 24000;
    s->sample_rate = 24000;
    s->data = (float*)calloc(s->max, sizeof(float));

    for (int i = 0; i < s->max; i++) {
        s->data[i] = sin(TWO_PI * ((double)i / s->sample_rate) * hz);
    }
}

static void init_audio_src_white_noise(int id)
{
    struct src_s *s = &src[id];

    s->max = 24000;
    s->sample_rate = 24000;
    s->data = (float*)calloc(s->max, sizeof(float));

    for (int i = 0; i < s->max; i++) {
        s->data[i] =  2 * (((double)random() / RAND_MAX) - 0.5);
    }
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
        ERROR("num_chan must be 1\n");
        exit(1);
    }

    // xxx cleanup

    double min, max, avg;
    average_float(data, num_items, &min, &max, &avg);
    fprintf(stderr, "min, max, avg = %f %f %f\n", min, max, avg);

    double scale;
    if (-min > max) {
        scale = 1. / -min;
    } else {
        scale = 1. / max;
    }
    for (int i = 0; i < num_items; i++) {
        data[i] *= scale;
    }

    average_float(data, num_items, &min, &max, &avg);
    fprintf(stderr, "min, max, avg = %f %f %f\n", min, max, avg);

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

double lpf(double v, int k1, double k2)
{
    static double cx[20];
    return low_pass_filter_ex(v, cx, k1, k2);
}

// -----------------  FAST SINE WAVE  ------------------

#define MAX_SINE_WAVE 2000
static double sine[MAX_SINE_WAVE];

void init_sine_wave(void)
{
    for (int i = 0; i < MAX_SINE_WAVE; i++) {
        sine[i] = sin((TWO_PI / MAX_SINE_WAVE) * i);
    }
}

double sine_wave(double f, double t)
{
    double iptr;
    int idx;

    idx = modf(f*t, &iptr) * MAX_SINE_WAVE;
    if (idx < 0 || idx >= MAX_SINE_WAVE) {
        ERROR("sine_wave idx %d\n", idx); //xxx del
        exit(1);
    }
    return sine[idx];
}

// -----------------  XXXXXXXXXXXXXX  ------------------

double moving_avg(double v, int n, void **cx_arg)
{
    struct {
        double sum;
        int    idx;
        double vals[0];
    } *cx;

    if (*cx_arg == NULL) {
        int len = sizeof(*cx) + n*sizeof(double);
        *cx_arg = malloc(len);
        memset(*cx_arg, 0, len);
    }
    cx = *cx_arg;

    cx->sum += (v - cx->vals[cx->idx]);
    cx->vals[cx->idx] = v;
    if (++(cx->idx) == n) cx->idx = 0;
    return cx->sum / n;
}

void average_float(float *v, int n, double *min_arg, double *max_arg, double *avg)
{
    double sum = 0;
    double min = 1e99;
    double max = -1e99;

    for (int i = 0; i < n; i++) {
        sum += v[i];
        if (v[i] < min) min = v[i];
        if (v[i] > max) max = v[i];
    }
    *avg = sum / n;
    *min_arg = min;
    *max_arg = max;
}

void average(double *v, int n, double *min_arg, double *max_arg, double *avg)
{
    double sum = 0;
    double min = 1e99;
    double max = -1e99;

    for (int i = 0; i < n; i++) {
        sum += v[i];
        if (v[i] < min) min = v[i];
        if (v[i] > max) max = v[i];
    }
    *avg = sum / n;
    *min_arg = min;
    *max_arg = max;
}

void normalize(double *v, int n, double min, double max)
{
    double span = max - min;
    double vmin, vmax, vavg, vspan;

    average(v, n, &vmin, &vmax, &vavg);
    vspan = vmax - vmin;
    NOTICE("normalize  %f %f\n", vmin, vmax);

    for (int i = 0; i < n; i++) {
        //v[i] = (v[i] + (min - vmin)) * (span / vspan);
        v[i] = (v[i] - vmin) * (span / vspan) + min;
    }

    //average(v, n, &vmin, &vmax, &vavg);
    //NOTICE("   now %f %f\n", vmin, vmax);
}
