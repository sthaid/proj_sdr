#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <misc.h>
#include <fft.h>
#include <filter.h>
#include <wav.h>

//
// defines
//

#define MAX_STATION 20

#define T_MAX 30   // duration of simulated data, in secs

#define AM 0
#define FM 1
#define LSB 2
#define USB 3

#define SDR_SAMPLE_RATE 2400000   // 2.4 MS/sec
#define DELTA_T         (1. / SDR_SAMPLE_RATE)

#define F_LPF     (modulation == FM ? 20000 : 4000)
#define LPF_ORDER 20

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

static void init_antenna(void);
static double get_antenna(double t);
static double get_station_signal(int id, double t);

// -----------------  ANTENNA TEST  -------------------------

int main(int argc, char **argv)
{
    double  *antenna, t;
    FILE    *fp;
    int      n = 0;
    int      max_n = SDR_SAMPLE_RATE;

    init_antenna();

    antenna = calloc(max_n, sizeof(double));

    // open antenna data file for writing
    fp = fopen("sim.dat", "w");
    if (fp == NULL) {
        FATAL("failed to create sim.dat, %m\n");
    }

    // loop over duration over simulated antenna data to be created
    for (t = 0; t < T_MAX; t += DELTA_T) {
        // get simulaed antenna data for time 't'
        antenna[n++] = get_antenna(t);

        // write the data fo the file
        if (n == max_n) {
            NOTICE("Generating simulated antenna data: %0.1f secs", t);
            fwrite(antenna, sizeof(double), n, fp);
            n = 0;
        }
    }

    NOTICE("Done");

    // close file
    fclose(fp);

    return 0;
}

static double get_antenna(double t)
{
    double y = 0;

    for (int i = 0; i < max_station; i++) {
        y += get_station_signal(i, t);
    }
    return y;
}

// -----------------  INIT ANTENNA SIGNAL  -----------------------------

static void init_station_wav_file(int modulation, double carrier_freq, double carrier_amp, char *filename) ATTRIB_UNUSED;
static void init_station_sine_wave(int modulation, double carrier_freq, double carrier_amp, double sine_wave_freq) ATTRIB_UNUSED;
static void init_station_white_noise(int modulation, double carrier_freq, double carrier_amp) ATTRIB_UNUSED;

static void init_antenna(void)
{
    init_station_wav_file(   AM,  520000,   1, "wav_files/one_bourbon_one_scotch_one_beer.wav");
    init_station_wav_file(   AM,  540000,   1, "wav_files/super_critical.wav");
    init_station_wav_file(   AM,  560000,   1, "wav_files/proud_mary.wav");
    //init_station_wav_file(   USB, 620000,   4, "wav_files/blue_sky.wav");
    //init_station_wav_file(   LSB, 640000,   4, "wav_files/blue_sky.wav");
    init_station_wav_file(   FM,  1000000, 15, "wav_files/not_fade_away.wav");
}

static double get_station_signal(int id, double t)
{
    int idx, mod;
    double signal, A, fc, wc, data;
    struct station_s *a;

    static BWLowPass *bwi[99], *bwq[99];

    a    = &station[id];
    idx  = (unsigned long)nearbyint(t * a->audio_sample_rate) % a->audio_n;
    data = a->audio_data[idx];
    A    = a->carrier_amp;
    fc   = a->carrier_freq;
    wc   = TWO_PI * fc;
    mod  = a->modulation;

    if (bwi[id] == NULL && (mod == USB || mod == LSB)) {
        bwi[id] = create_bw_low_pass_filter(LPF_ORDER, SDR_SAMPLE_RATE, 2000);
        bwq[id] = create_bw_low_pass_filter(LPF_ORDER, SDR_SAMPLE_RATE, 2000);
    }

    switch (mod) {
    case AM: {
        // xxx comment
        signal = (1 + data) * (A * sin(wc * t));
        break; }
    case FM: {
        // xxx comment
        #define KHZ 1000
        const double f_delta = 100*KHZ;
        a->fm_data_integral += data * DELTA_T;
        signal = A * cos(wc * t + TWO_PI * f_delta * a->fm_data_integral);
        break; }
    case USB: case LSB: {
        // Weaver modulator
        // https://en.wikipedia.org/wiki/Single-sideband_modulation
        // https://panoradio-sdr.de/ssb-demodulation/
        complex tmp;
        double sign = (mod == USB ? -1 : 1);
        tmp = data * cexp(sign * I * (TWO_PI*2000) * t);
        tmp = bw_low_pass(bwi[id], creal(tmp)) + 
              bw_low_pass(bwq[id], cimag(tmp)) * I;
        tmp = tmp * cexp(I * wc * t);
        signal = A * creal(tmp);
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
        ERROR("read_wav_file %s, %m\n", filename);
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

