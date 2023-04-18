#include "common.h"

// xxx comments, format
// -----------------  GET ANTENNA SIGNAL  ------------------------------

//#define MAX_AUDIO 5
//#define F_DELTA   20000
//#define MAX_AUDIO 12
//#define F_DELTA   100000
#define MAX_AUDIO 1
#define F_DELTA   100000

#define F_LPF 4000

static double get_audio(int id, double t);
static void init_audio_wav_file(int id, char *filename);
static void init_audio_sine_wave(int id, int f);
static void init_audio_white_noise(int id, double peak);

double get_antenna(double t, double f_center)
{
    double f, y, A;
    int i;

    y = 0;
    f = f_center - F_DELTA * ((MAX_AUDIO/2.) - 0.5);

    for (i = 0; i < MAX_AUDIO; i++) {
        A = (double)(i + 1) / MAX_AUDIO;
        y += (1 + get_audio(i,t)) * (A * sin(TWO_PI * f * t));
        f += F_DELTA;
    }

    return y;
}

void init_antenna(void)
{
    init_audio_sine_wave(0, 300);

#if 0
    init_audio_wav_file(0, "proud_mary.wav");
    init_audio_wav_file(1, "one_bourbon_one_scotch_one_beer.wav");
    init_audio_wav_file(2, "super_critical.wav");
    init_audio_sine_wave(3, 300);
    init_audio_white_noise(4);
#else
//  for (int i = 0; i < MAX_AUDIO; i++) {
//      init_audio_white_noise(i, 1);
//  }
#endif
}

// - - - - - - - - - - - - - - - - - - - - 

static struct audio_s {
    int n;
    int sample_rate;
    double *data;
} audio[MAX_AUDIO];

static double get_audio(int id, double t)
{
    int idx;
    struct audio_s *a = &audio[id];

    idx = (unsigned long)nearbyint(t * a->sample_rate) % a->n;
    return a->data[idx];
}

static void init_audio_wav_file(int id, char *filename)
{
    int    ret, num_chan, num_items, sample_rate;
    struct audio_s *a = &audio[id];
    double *data;

    ret = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0) {
        ERROR("read_wav_file %s, %s\n", filename, strerror(errno));
        exit(1);
    }
    NOTICE("num_chan=%d num_items=%d sample_rate=%d\n", num_chan, num_items, sample_rate);

    if (num_chan != 1) {
        ERROR("num_chan must be 1\n");
        exit(1);
    }

    a->n         = num_items;
    a->sample_rate = sample_rate;
    a->data        = data;

    fft_lpf_real(a->data, a->data, a->n, a->sample_rate, F_LPF);
    normalize(a->data, a->n, -1, 1);
}

static void init_audio_sine_wave(int id, int f)
{
    struct audio_s *a = &audio[id];
    double          w = TWO_PI * f;
    double          t, dt;
    int             i;

    a->n = 24000;
    a->sample_rate = 24000;
    a->data = (double*)calloc(a->n, sizeof(double));

    t = 0;
    dt = 1. / a->sample_rate;

    for (i = 0; i < a->n; i++) {
        a->data[i] = sin(w * t);
        t += dt;
    }
}

static void init_audio_white_noise(int id, double peak)
{
    struct audio_s *a = &audio[id];

    a->n = 24000;
    a->sample_rate = 24000;
    a->data = (double*)calloc(a->n, sizeof(double));

    for (int i = 0; i < a->n; i++) {
        a->data[i] =  ((double)random() / RAND_MAX) - 0.5;
    }

    fft_lpf_real(a->data, a->data, a->n, a->sample_rate, F_LPF);
    normalize(a->data, a->n, -peak, peak);
}

