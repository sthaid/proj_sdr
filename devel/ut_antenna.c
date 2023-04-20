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
    //init_station_sine_wave(400000, 0.75, 3700);
    //init_station_sine_wave(600000, 1.00, 3700);
    //init_station_sine_wave(800000, 1.00, 3700);

    //init_station_sine_wave(600000, 1.00, 300);
    init_station_wav_file( 600000, 1, "super_critical.wav");

#if 0
    init_station_wav_file( 800000, 1, "super_critical.wav");
#endif
#if 0
    init_station_wav_file(0, "proud_mary.wav");
    init_station_wav_file(1, "one_bourbon_one_scotch_one_beer.wav");
    init_station_wav_file(2, "super_critical.wav");
    init_station_sine_wave(3, 300);
    init_station_white_noise(4);
#endif
#if 0
    for (int i = 0; i < 12; i++) {
        init_station_white_noise(i, 1);
    }
#endif
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
    double *audio_data;

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

    a->audio_n           = num_items;
    a->audio_sample_rate = sample_rate;
    a->audio_data        = audio_data;
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

    a->audio_n           = 100000;
    a->audio_sample_rate = 100000; //xxx also for white noise
    a->audio_data        = (double*)calloc(a->audio_n, sizeof(double));
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

    a->audio_n           = 100000;
    a->audio_sample_rate = 100000;
    a->audio_data        = (double*)calloc(a->audio_n, sizeof(double));
    a->carrier_freq      = carrier_freq;
    a->carrier_amp       = carrier_amp;

    for (int i = 0; i < a->audio_n; i++) {
        a->audio_data[i] =  ((double)random() / RAND_MAX) - 0.5;
    }

    fft_lpf_real(a->audio_data, a->audio_data, a->audio_n, a->audio_sample_rate, F_LPF);
    normalize(a->audio_data, a->audio_n, -1, 1);
}

