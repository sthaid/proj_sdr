#include "common.h"

//
// defines
//

#define T_MAX 30   // duration of antenna.dat
#define T_FFT 0.1  // duration of each fft

#define MAX_STATION 20

#define F_LPF  (modulation == FM ? 20000 : 4000)

#define AM 0
#define FM 1
#define LSB 2
#define USB 3

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

void *antenna_test(void *cx)
{
    double  *antenna, t;
    complex *antenna_fft;
    FILE    *fp;
    int      n = 0;
    int      max_n = nearbyint(SAMPLE_RATE * T_FFT);

    init_antenna();

    antenna     = fftw_alloc_real(max_n);
    antenna_fft = fftw_alloc_complex(max_n);

    // open antenna data file for writing
    fp = fopen(ANTENNA_FILENAME, "w");
    if (fp == NULL) {
        FATAL("failed to create %s\n", ANTENNA_FILENAME);
    }

    // loop over duration over simulated antenna data to be created
    for (t = 0; t < T_MAX; t += DELTA_T) {
        // get simulaed antenna data for time 't'
        antenna[n++] = get_antenna(t);

        // if have accumulaed T_FFT (0.1s) of antenna data then
        // - compute and plot fft of the 0.1s of data
        // - write the data fo the file
        if (n == max_n) {
            sprintf(tc.info, "Generating simulated antenna data: %0.1f secs", t);

            fft_fwd_r2c(antenna, antenna_fft, n);
            plot_fft(0, antenna_fft, n, SAMPLE_RATE,
                     0, SAMPLE_RATE/2, 0, NOC, NOC, "ANTENNA_FFT",   
                     0, 0, 50, 50);

            fwrite(antenna, sizeof(double), n, fp);

            n = 0;
        }
    }

    sprintf(tc.info, "Done");

    // close file
    fclose(fp);

    // push SDL_EVENT_QUIT to cause the program to terminate
    sdl_push_event(&(sdl_event_t){SDL_EVENT_QUIT});

    return NULL;
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
    init_station_white_noise(AM,  420000, 1);
    init_station_wav_file(   AM,  460000, 1, "wav_files/one_bourbon_one_scotch_one_beer.wav");
    init_station_wav_file(   AM,  500000, 1, "wav_files/super_critical.wav");
    init_station_wav_file(   AM,  540000, 1, "wav_files/proud_mary.wav");
    init_station_wav_file(   USB, 580000, 4, "wav_files/blue_sky.wav");
    init_station_wav_file(   LSB, 620000, 4, "wav_files/blue_sky.wav");
    init_station_wav_file(   FM,  800000, 15, "wav_files/not_fade_away.wav");
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
        bwi[id] = create_bw_low_pass_filter(20, SAMPLE_RATE, 2000);
        bwq[id] = create_bw_low_pass_filter(20, SAMPLE_RATE, 2000);
    }

    switch (mod) {
    case AM: {
        // xxx comment
        signal = (1 + data) * (A * sin(wc * t));
        break; }
    case FM: {
        // xxx comment
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

