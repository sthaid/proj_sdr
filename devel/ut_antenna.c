#include "common.h"

//
// defines
//

#define MAX_STATION 20
#define F_LPF     4000

#define AM 1
#define FM 2

//
// variables
//

static int max_station;

static struct station_s {
    // params
    int     audio_n;
    int     audio_sample_rate;
    double *audio_data;
    double  carrier_freq;
    double  carrier_amp;
    int     modulation;
    // variables
    double  fm_data_integral;
} station[MAX_STATION];

//
// prototypes
//

static double get_station_signal(int id, double t);
static void init_station_wav_file(int modulation, double carrier_freq, double carrier_amp, char *filename);
static void init_station_sine_wave(int modulation, double carrier_freq, double carrier_amp, double sine_wave_freq);
static void init_station_white_noise(int modulation, double carrier_freq, double carrier_amp);

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
    init_station_white_noise(AM, 420000, 1);
    init_station_wav_file(   AM, 460000, 1, "wav_files/one_bourbon_one_scotch_one_beer.wav");
    init_station_wav_file(   AM, 500000, 1, "wav_files/super_critical.wav");
    init_station_wav_file(   AM, 540000, 1, "wav_files/proud_mary.wav");
    init_station_sine_wave(  AM, 580000, 1, 500);

    init_station_wav_file(   FM, 800000, 1, "wav_files/not_fade_away.wav");
}

// -----------------  GET STATION  -------------------------------------

static double get_station_signal(int id, double t)
{
    int idx, mod;
    double signal, A, fc, wc, data;
    struct station_s *a;

    a    = &station[id];
    idx  = (unsigned long)nearbyint(t * a->audio_sample_rate) % a->audio_n;
    data = a->audio_data[idx];
    A    = a->carrier_amp;
    fc   = a->carrier_freq;
    wc   = TWO_PI * fc;
    mod  = a->modulation;

    switch (mod) {
    case AM: {
        signal = (1 + data) * (A * sin(wc * t));
        break; }
    case FM: {
        const double delta_t = (1. / 2400000);  // xxx should be in common.h
        const double f_delta = 100000;          // 100 KHz ?
        a->fm_data_integral += data * delta_t;
        signal = A * cos(wc * t + TWO_PI * f_delta * a->fm_data_integral);
        break; }
    default:
        FATAL("invalid modulation %d\n", mod);
        break;
    }

    return signal;
}

static void init_station_wav_file(int modulation, double carrier_freq, double carrier_amp, char *filename)
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

    a->audio_n           = num_items;     // duration is length of wav file
    a->audio_sample_rate = sample_rate;
    a->audio_data        = audio_data2;
    a->carrier_freq      = carrier_freq;
    a->carrier_amp       = carrier_amp;
    a->modulation        = modulation;

    fft_lpf_real(a->audio_data, a->audio_data, a->audio_n, a->audio_sample_rate, F_LPF);
    normalize(a->audio_data, a->audio_n, -1, 1);
}

static void init_station_sine_wave(int modulation, double carrier_freq, double carrier_amp, double sine_wave_freq)
{
    struct station_s *a = &station[max_station++];
    double          w = TWO_PI * sine_wave_freq;
    double          t, dt;
    int             i;

    a->audio_n           = 22000;   // 1 sec
    a->audio_sample_rate = 22000;
    a->audio_data        = fftw_alloc_real(a->audio_n+2);
    a->carrier_freq      = carrier_freq;
    a->carrier_amp       = carrier_amp;
    a->modulation        = modulation;

    t = 0;
    dt = 1. / a->audio_sample_rate;

    for (i = 0; i < a->audio_n; i++) {
        a->audio_data[i] = sin(w * t);
        t += dt;
    }

    fft_lpf_real(a->audio_data, a->audio_data, a->audio_n, a->audio_sample_rate, F_LPF);
    normalize(a->audio_data, a->audio_n, -1, 1);
}

static void init_station_white_noise(int modulation, double carrier_freq, double carrier_amp)
{
    struct station_s *a = &station[max_station++];

    a->audio_n           = 10*22000;  // 10 sec
    a->audio_sample_rate = 22000;;
    a->audio_data        = fftw_alloc_real(a->audio_n+2);
    a->carrier_freq      = carrier_freq;
    a->carrier_amp       = carrier_amp;
    a->modulation        = modulation;

    for (int i = 0; i < a->audio_n; i++) {
        a->audio_data[i] =  ((double)random() / RAND_MAX) - 0.5;
    }

    fft_lpf_real(a->audio_data, a->audio_data, a->audio_n, a->audio_sample_rate, F_LPF);
    normalize(a->audio_data, a->audio_n, -1, 1);
}

