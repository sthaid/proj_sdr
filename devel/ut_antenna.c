#include "common.h"

//
// defines
//

#define MAX_STATION 20
#define F_LPF     4000

//
// variables
//

static int max_station;

static struct station_s {
    int     audio_n;
    int     audio_sample_rate;
    double *audio_data;
    double  carrier_freq;
    double  carrier_amp;
} station[MAX_STATION];

//
// prototypes
//

static double get_station_signal(int id, double t);
static void init_station_wav_file(double carrier_freq, double carrier_amp, char *filename);
static void init_station_sine_wave(double carrier_freq, double carrier_amp, double sine_wave_freq);
static void init_station_white_noise(double carrier_freq, double carrier_amp);

// -----------------  GET ANTENNA SIGNAL  ------------------------------

double get_antenna(double t)
{
    double y = 0;

    for (int i = 0; i < max_station; i++) {
        y += get_station_signal(i, t);
    }
    return y;
}

void init_antenna(void)
{
    init_station_white_noise(420000, .2);
    init_station_wav_file(   460000, .4, "wav_files/one_bourbon_one_scotch_one_beer.wav");
    init_station_wav_file(   500000, .6, "wav_files/super_critical.wav");
    init_station_wav_file(   540000, .8, "wav_files/proud_mary.wav");
    init_station_sine_wave(  580000, 1, 500);
}

// -----------------  GET STATION  -------------------------------------

static double get_station_signal(int id, double t)
{
    int audio_idx;
    double signal;
    struct station_s *a = &station[id];

    audio_idx = (unsigned long)nearbyint(t * a->audio_sample_rate) % a->audio_n;
    signal = (1 + a->audio_data[audio_idx]) * (a->carrier_amp * sin(TWO_PI * a->carrier_freq * t));

    return signal;
}

static void init_station_wav_file(double carrier_freq, double carrier_amp, char *filename)
{
    int    ret, num_chan, num_items, sample_rate;
    struct station_s *a = &station[max_station++];
    double *audio_data, *audio_data2;

    ret = read_wav_file(filename, &audio_data, &num_chan, &num_items, &sample_rate);
    if (ret != 0) {
        ERROR("read_wav_file %s, %s\n", filename, strerror(errno));
        exit(1);
    }
    NOTICE("num_chan=%d num_items=%d sample_rate=%d\n", num_chan, num_items, sample_rate);

    if (num_chan != 1) {
        ERROR("num_chan must be 1\n");
        exit(1);
    }

    audio_data2 = fftw_alloc_real(num_items+2);
    memcpy(audio_data2, audio_data, num_items*sizeof(double));
    free(audio_data);

    a->audio_n           = num_items;
    a->audio_sample_rate = sample_rate;
    a->audio_data        = audio_data2;
    a->carrier_freq      = carrier_freq;
    a->carrier_amp       = carrier_amp;

    fft_lpf_real(a->audio_data, a->audio_data, a->audio_n, a->audio_sample_rate, F_LPF);
    normalize(a->audio_data, a->audio_n, -1, 1);
}

static void init_station_sine_wave(double carrier_freq, double carrier_amp, double sine_wave_freq)
{
    struct station_s *a = &station[max_station++];
    double          w = TWO_PI * sine_wave_freq;
    double          t, dt;
    int             i;

    a->audio_n           = 22000;
    a->audio_sample_rate = 22000;
    a->audio_data        = fftw_alloc_real(a->audio_n+2);
    a->carrier_freq      = carrier_freq;
    a->carrier_amp       = carrier_amp;

    t = 0;
    dt = 1. / a->audio_sample_rate;

    for (i = 0; i < a->audio_n; i++) {
        a->audio_data[i] = sin(w * t);
        t += dt;
    }

    fft_lpf_real(a->audio_data, a->audio_data, a->audio_n, a->audio_sample_rate, F_LPF);
    normalize(a->audio_data, a->audio_n, -1, 1);
}

static void init_station_white_noise(double carrier_freq, double carrier_amp)
{
    struct station_s *a = &station[max_station++];

    a->audio_n           = 10*22000;
    a->audio_sample_rate = 22000;;
    a->audio_data        = fftw_alloc_real(a->audio_n+2);
    a->carrier_freq      = carrier_freq;
    a->carrier_amp       = carrier_amp;

    for (int i = 0; i < a->audio_n; i++) {
        a->audio_data[i] =  ((double)random() / RAND_MAX) - 0.5;
    }

    fft_lpf_real(a->audio_data, a->audio_data, a->audio_n, a->audio_sample_rate, F_LPF);
    normalize(a->audio_data, a->audio_n, -1, 1);
}

