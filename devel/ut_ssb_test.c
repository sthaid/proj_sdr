#include "common.h"

#define USB 0
#define LSB 1

// config settings
static const int sample_rate = 40000;
static const int f           = 10000;
static const int n           = 10 * sample_rate;  // 10 sec

// test control 
static double    tc_step        = 0;
static double    tc_which       = USB;
static double    tc_freq_offset = 0;

// prototypes
static void init_msg(double *msg, int n);

// - - - - - - - - - - - - - - - - - - - - - 

void *ssb_test(void *cx)
{
    complex *yc, *fft;
    double  *yr;
    double  *msg;
    double   t;
    double   yv_max;
    double   curr_step;
    double   curr_freq_offset;
    double   delta_t = (1. / sample_rate);

    t = 0;
    yv_max = 2500;  //xxx determine automatically
    delta_t = (1. / sample_rate);

    msg = calloc(n, sizeof(double));
    init_msg(msg, n);

    tc.ctrl[0] = (struct test_ctrl_s)
                 {"STEP", &tc_step, 0, 6, 1,
                  {}, NULL,
                  SDL_EVENT_KEY_DOWN_ARROW, SDL_EVENT_KEY_UP_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {NULL, &tc_which, 0, 1, 1,
                  {"USB", "LSB"}, NULL,
                  'u', 'l'};
    tc.ctrl[2] = (struct test_ctrl_s)
                 {NULL, &tc_freq_offset, -10000, 10000, 100,
                  {}, "HZ",
                  SDL_EVENT_KEY_LEFT_ARROW, SDL_EVENT_KEY_RIGHT_ARROW};

    yc = fftw_alloc_complex(n);
    yr = fftw_alloc_real(n);
    fft = fftw_alloc_complex(n);

    BWLowPass *bwi, *bwq;
    bwi = create_bw_low_pass_filter(20, sample_rate, 2000);
    bwq = create_bw_low_pass_filter(20, sample_rate, 2000);

    BWLowPass *bwi2, *bwq2;
    bwi2 = create_bw_low_pass_filter(20, sample_rate, 2000);
    bwq2 = create_bw_low_pass_filter(20, sample_rate, 2000);

    while (true) {
        tc.info[0] = '\0';
        curr_step = tc_step;
        curr_freq_offset = tc_freq_offset;

        for (int i = 0; i < n; i++) {
            complex tmp = 0;
            double m = msg[i];

            // --- modulate ---

            if (curr_step >= 0) {
                if (curr_step == 0) 
                    sprintf(tc.info, "msg data");
                tmp = m;
            }

            if (curr_step >= 1) {
                if (curr_step == 1) 
                    sprintf(tc.info, "modulating - msg shifted to f=0");
                int sign = (tc_which == USB ? -1 : 1);
                tmp = tmp * cexp(sign * I * (TWO_PI*2000) * t);
            }

            if (curr_step >= 2) {
                if (curr_step == 2) 
                    sprintf(tc.info, "modulating - msg shifted to f=0 and filtered");
                tmp = bw_low_pass(bwi, creal(tmp)) +
                      bw_low_pass(bwq, cimag(tmp)) * I;
            }

            if (curr_step >= 3) {
                if (curr_step == 3) 
                    sprintf(tc.info, "modulating - msg shifted to f=0 and filtered and shifted to f");
                tmp = tmp * cexp(I * (TWO_PI*f) * t);
                tmp = creal(tmp);  // xxx try adding cimag, and in antenna.c
            }

            // --- demod ---

            if (curr_step >= 4) {
                if (curr_step == 4) 
                    sprintf(tc.info, "demodulating - shift back to f=0");
                tmp = tmp * cexp(-I * (TWO_PI*(f+curr_freq_offset)) * t);
            }

            if (curr_step >= 5) {
                if (curr_step == 5) 
                    sprintf(tc.info, "demodulating - shift back to f=0 and filtered");
                tmp = bw_low_pass(bwi2, creal(tmp)) +
                      bw_low_pass(bwq2, cimag(tmp)) * I;
            }

            if (curr_step >= 6) {
                if (curr_step == 6) 
                    sprintf(tc.info, "demodulating - shift back to f=0 and filtered and shifted to f=2000");
                int sign = (tc_which == USB ? 1 : -1);
                tmp = tmp * cexp(sign * I * (TWO_PI*2000) * t);
            }

            // save the real and complex results, to be used below
            // to plot fft and to output audio
            yr[i] = creal(tmp) + cimag(tmp);  // xxx try with and without cimag
            yc[i] = tmp;

            // advance time
            t += delta_t;
        }

        // plot the complex fft
        fft_fwd_c2c(yc, fft, n);
        plot_fft(0, fft, n, sample_rate, false, yv_max, 0, "FFT_COMPLEX", 0, 0, 100, 25);

        // plot the real fft, on steps where it is appropriate
        if (curr_step == 0 || curr_step == 3 || curr_step == 6) {
            zero_complex(fft,n);
            fft_fwd_r2c(yr, fft, n);
            plot_fft(1, fft, n, sample_rate, false, yv_max, 0, "FFT_REAL", 0, 25, 100, 25);
        } else {
            plot_clear(1);
        }

        // play the audio, until the step or frequency offset has changed
        if (curr_step == 0 || curr_step == 6) {
            NOTICE("playing\n");
            static int xx;
            while (tc_step == curr_step && tc_freq_offset == curr_freq_offset) {
                audio_out(yr[xx++]);
                if (xx == n) xx = 0;
            }
        }
    }

    return NULL;
}

static void init_msg(double *wav, int n)
{
    char *filename = "wav_files/blue_sky.wav";
    int ret, num_chan, num_items, sample_rate;
    double *data;

    ret = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0 || num_chan != 1 || sample_rate < 20000 || sample_rate > 24000) {
        FATAL("read_wav_file ret=%d num_chan=%d sample_rate=%d\n", ret, num_chan, sample_rate);
    }

    for (int i = 0; i < n; i++) {
        wav[i] = data[i%num_items];
    }
    
    free(data);
}   

