#include "common.h"

// xxx next do lsb

#define USB 0
#define LSB 1

static int sample_rate = 40000;

static double msg(double t);

void *ssb_test(void *cx)
{
    complex *yc, *fft;
    double   *yr;
    double   t       = 0;
    int      n       = 1 * sample_rate;  // 1 sec
    double   delta_t = (1. / sample_rate);
    double   yv_max  = 20000;
    double   step = 0;
    double   which = USB;

    tc.ctrl[0] = (struct test_ctrl_s)
                 {"STEP", &step, 0, 6, 1,
                  {}, NULL,
                  SDL_EVENT_KEY_DOWN_ARROW, SDL_EVENT_KEY_UP_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {NULL, &which, 0, 1, 1,
                  {"USB", "LSB"}, NULL,
                  'u', 'l'};

    yc = fftw_alloc_complex(n);
    yr = fftw_alloc_real(n);
    fft = fftw_alloc_complex(n);

    BWLowPass *bwi, *bwq;
    bwi = create_bw_low_pass_filter(20, sample_rate, 2000);
    bwq = create_bw_low_pass_filter(20, sample_rate, 2000);

    BWLowPass *bwi2, *bwq2;
    bwi2 = create_bw_low_pass_filter(20, sample_rate, 2000);
    bwq2 = create_bw_low_pass_filter(20, sample_rate, 2000);

again:
    for (int i = 0; i < n; i++) {
        complex tmp = 0;
        double m = msg(t);

        if (step >= 0)
            tmp = m;

        if (step >= 1) {
            int sign = (which == USB ? -1 : 1);
            tmp = tmp * cexp(sign * I * (TWO_PI*2000) * t);
        }

        if (step >= 2)
            tmp = bw_low_pass(bwi, creal(tmp)) +
                  bw_low_pass(bwq, cimag(tmp)) * I;

        if (step >= 3) 
            tmp = tmp * cexp(I * (TWO_PI*10000) * t);

        // --- demod ---

        if (step >= 4) 
            tmp = tmp * cexp(-I * (TWO_PI*10000) * t);

        if (step >= 5)
            tmp = bw_low_pass(bwi2, creal(tmp)) +
                  bw_low_pass(bwq2, cimag(tmp)) * I;

        if (step >= 6) {
            int sign = (which == USB ? 1 : -1);
            tmp = tmp * cexp(sign * I * (TWO_PI*2000) * t);
        }

        yr[i] = creal(tmp);
        yc[i] = tmp;

        t += delta_t;
    }

    fft_fwd_c2c(yc, fft, n);
    plot_fft(0, fft, n, sample_rate, false, yv_max, 0, "FFT_COMPLEX", 0, 0, 100, 25);

    if (step == 0 || step == 3 || step == 6) {
        zero_complex(fft,n);
        fft_fwd_r2c(yr, fft, n);
        plot_fft(1, fft, n, sample_rate, false, yv_max, 0, "FFT_REAL", 0, 25, 100, 25);
    } else {
        plot_clear(1);
    }

    usleep(10000);
    goto again;

    return NULL;
}

static double msg(double t)
{
    double sum = 0, f, amp;

    // sum of sine waves with freq range 1000 to 3000 hz
    for (f = 100; f < 10000; f+=100) {
        amp = ((f - 100) / 10000);
        sum += amp * sin(TWO_PI * f * t);
    }

    return sum;
}

#if 0
// xxx try bpf too

static void init_using_sine_waves(double *sw, int n, int freq_first, int freq_last, int freq_step, int sample_rate);
static void init_using_white_noise(double *wn, int n);
static void init_using_wav_file(double *wav, int n, char *filename);

// -----------------  FILTER TEST  ------------------------

void *filter_test(void *cx)
{
    #define SINE_WAVE   0
    #define SINE_WAVES  1
    #define WHITE_NOISE 2
    #define WAV_FILE    3

    #define DEFAULT_CUTOFF 4000
    #define DEFAULT_ORDER  8

    int        sample_rate = 20000;               // 20 KS/s
    int        max         = 120 * sample_rate;   // 120 total secs of data
    int        n           = .1 * sample_rate;    // .1 secs of data processed at a time
    int        total       = 0;

    double     tc_mode = SINE_WAVES;
    double     tc_cutoff = DEFAULT_CUTOFF;
    double     tc_order  = DEFAULT_ORDER;
    double     tc_reset = 0;

    double    *in_real         = fftw_alloc_real(max);
    double    *in              = fftw_alloc_real(n);
    double    *in_filtered     = fftw_alloc_real(n);
    complex   *in_fft          = fftw_alloc_complex(n);
    complex   *in_filtered_fft = fftw_alloc_complex(n);

    BWLowPass *bwlpf = NULL;
    int        bwlpf_cutoff = 0;
    int        bwlpf_order  = 0;
    int        current_mode;

    double     yv_max;

    // init test controls
    tc.ctrl[0] = (struct test_ctrl_s)
                 {"MODE", &tc_mode, SINE_WAVE, WAV_FILE, 1, 
                  {"SINE_WAVE", "SINE_WAVES", "WHITE_NOISE", "WAV_FILE"}, "", 
                  SDL_EVENT_KEY_DOWN_ARROW, SDL_EVENT_KEY_UP_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {"CUTOFF", &tc_cutoff, 100, 10000, 100,
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW, SDL_EVENT_KEY_RIGHT_ARROW};
    tc.ctrl[2] = (struct test_ctrl_s)
                 {"ORDER", &tc_order, 1, 20, 1,
                  {}, NULL, 
                  SDL_EVENT_KEY_SHIFT_LEFT_ARROW, SDL_EVENT_KEY_SHIFT_RIGHT_ARROW};
    tc.ctrl[3] = (struct test_ctrl_s)
                 {"RESET", &tc_reset, 0, 1, 1,
                  {"", ""}, NULL, 
                  SDL_EVENT_NONE, 'r'};

    while (true) {
        // construct in buff from either:
        // - sine waves
        // - white noise
        // - wav file
        current_mode = tc_mode;
        switch (current_mode) {
        case SINE_WAVE:
            init_using_sine_waves(in_real, max, 1000, 1000, 0, sample_rate);
            yv_max = 1000;
            break;
        case SINE_WAVES:
            init_using_sine_waves(in_real, max, 0, 10000, 500, sample_rate);
            yv_max = 1000;
            break;
        case WHITE_NOISE:
            init_using_white_noise(in_real, max);
            yv_max = 30;
            break;
        case WAV_FILE:
            init_using_wav_file(in_real, max, "wav_files/super_critical.wav");
            yv_max = 15;
            break;
        default:
            FATAL("invalid current_mode %d\n", current_mode);
            break;
        }

        // loop until test-ctrl mode changes
        while (current_mode == tc_mode) {
            // reset ctrls when requested
            if (tc_reset) {
                tc_reset = 0;
                tc_cutoff = DEFAULT_CUTOFF;
                tc_order  = DEFAULT_ORDER;
            }

            // if test ctrl cutoff freq has changed then 
            // free and recreate the butterworth lpf
            if (tc_cutoff != bwlpf_cutoff || tc_order != bwlpf_order) {
                if (bwlpf != NULL) {
                    free_bw_low_pass(bwlpf);
                }
                bwlpf = create_bw_low_pass_filter(tc_order, sample_rate, tc_cutoff);
                bwlpf_cutoff = tc_cutoff;
                bwlpf_order  = tc_order;
            }

            // copy .1 secs of data from 'in_real' to 'in'
            for (int i = 0; i < n; i++) {
                in[i] = in_real[(total+i)%max];
            }
            total += n;

            // apply low pass filter of 'in', output to 'in_filtered'
            for (int i = 0; i < n; i++) {
                in_filtered[i] = bw_low_pass(bwlpf, in[i]);
            }

            // plot fft of 'in'
            fft_fwd_r2c(in, in_fft, n);
            plot_fft(0, in_fft, n, sample_rate, true, yv_max, tc_cutoff, "FFT", 0, 0, 50, 25);

            // plot fft of 'in_filtered'
            fft_fwd_r2c(in_filtered, in_filtered_fft, n);
            plot_fft(1, in_filtered_fft, n, sample_rate, true, yv_max, tc_cutoff, "FFT", 0, 25, 50, 25);

            // play the filtered audio
            for (int i = 0; i < n; i++) {
                audio_out(in_filtered[i]);
            }
        }
    }        
}

// -----------------  LOCAL ROUTINES  ---------------------

static void init_using_sine_waves(double *sw, int n, int freq_first, int freq_last, int freq_step, int sample_rate)
{
    int f;

    zero_real(sw,n);

    for (f = freq_first; f <= freq_last; f+= freq_step) {
        double  w = TWO_PI * f;
        double  t = 0;
        double  dt = (1. / sample_rate);

        for (int i = 0; i < n; i++) {
            sw[i] += (w ? sin(w * t) : 0.5);
            t += dt;
        }

        if (freq_step == 0) break;
    }
}

static void init_using_white_noise(double *wn, int n)
{
    for (int i = 0; i < n; i++) {
        wn[i] = ((double)random() / RAND_MAX) - 0.5;
    }
}

static void init_using_wav_file(double *wav, int n, char *filename)
{
    int ret, num_chan, num_items, sample_rate;
    double *data;

    ret = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0 || num_chan != 1 || sample_rate < 21000 || sample_rate > 23000) {
        FATAL("read_wav_file ret=%d num_chan=%d sample_rate=%d\n", ret, num_chan, sample_rate);
    }

    for (int i = 0; i < n; i++) {
        wav[i] = data[i%num_items];
    }
    
    free(data);
}   

#endif
