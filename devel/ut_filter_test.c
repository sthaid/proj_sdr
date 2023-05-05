#include "common.h"

// xxx try bpf too
// xxx 2nd plot cursor

#define WAV_FILENAME "wav_files/super_critical.wav"
//#define WAV_FILENAME "wav_files/blue_sky.wav"

static void init_using_sine_waves(double *sw, int n, int freq_first, int freq_last, int freq_step, int sample_rate, double scale);
static void init_using_white_noise(double *wn, int n, double scale);
static void add_white_noise(double *data, int n, double scale);
static void init_using_wav_file(double *wav, int n, char *filename, double scale);

// -----------------  FILTER TEST  ------------------------

void *filter_test(void *cx)
{
    #define SINE_WAVE                  0
    #define SINE_WAVES                 1
    #define WHITE_NOISE                2
    #define WAV_FILE                   3
    #define WAV_FILE_PLUS_WHITE_NOISE  4

    #define DEFAULT_CUTOFF 4000
    #define DEFAULT_ORDER  8

    int        sample_rate = 20000;               // 20 KS/s
    int        max         = 120 * sample_rate;   // 120 total secs of data
    int        n           = .1 * sample_rate;    // .1 secs of data processed at a time
    int        total       = 0;

    double     tc_mode = SINE_WAVE;
    double     tc_cutoff = DEFAULT_CUTOFF;
    double     tc_order  = DEFAULT_ORDER;
    double     tc_reset = 0;

    double    *in_real_total   = fftw_alloc_real(max);
    double    *in_real         = fftw_alloc_real(n);
    double    *in_filtered     = fftw_alloc_real(n);
    complex   *in_fft          = fftw_alloc_complex(n);
    complex   *in_filtered_fft = fftw_alloc_complex(n);

    BWLowPass *bwlpf = NULL;
    int        bwlpf_cutoff = 0;
    int        bwlpf_order  = 0;

    int        current_mode;
    double     scale, scale_wn, yv_max;

    // init test controls
    tc.ctrl[0] = (struct test_ctrl_s)
                 {"MODE", &tc_mode, SINE_WAVE, WAV_FILE_PLUS_WHITE_NOISE, 1, 
                  {"SINE_WAVE", "SINE_WAVES", "WHITE_NOISE", "WAV_FILE", "WAV+NOISE"}, "", 
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
        yv_max = 100;

        current_mode = tc_mode;
        switch (current_mode) {
        case SINE_WAVE:
            scale = 0.05;
            init_using_sine_waves(in_real_total, max, 1000, 1000, 0, sample_rate, scale);
            break;
        case SINE_WAVES:
            scale = 0.05;
            init_using_sine_waves(in_real_total, max, 0, 10000, 500, sample_rate, scale);
            break;
        case WHITE_NOISE:
            scale = 0.5;
            init_using_white_noise(in_real_total, max, scale);
            break;
        case WAV_FILE:
            scale = 4.0;
            init_using_wav_file(in_real_total, max, WAV_FILENAME, scale);
            break;
        case WAV_FILE_PLUS_WHITE_NOISE:
            scale = 4.0;
            scale_wn = 0.1;
            init_using_wav_file(in_real_total, max, WAV_FILENAME, scale);
            add_white_noise(in_real_total, max, scale_wn);
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

            // copy .1 secs of data from 'in_real_total' to 'in'
            for (int i = 0; i < n; i++) {
                in_real[i] = in_real_total[(total+i)%max];
            }
            total += n;

            // apply low pass filter of 'in', output to 'in_filtered'
            for (int i = 0; i < n; i++) {
                in_filtered[i] = bw_low_pass(bwlpf, in_real[i]);
            }

            // plot fft of 'in_real'
            fft_fwd_r2c(in_real, in_fft, n);
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

static void init_using_sine_waves(double *sw, int n, int freq_first, int freq_last, int freq_step, int sample_rate,
                                  double scale)
{
    int f;
    int nr_sine_waves=0;

    zero_real(sw,n);

    for (f = freq_first; f <= freq_last; f+= freq_step) {
        double  w = TWO_PI * f;
        double  t = 0;
        double  dt = (1. / sample_rate);

        for (int i = 0; i < n; i++) {
            sw[i] += (w ? sin(w * t) : 0.5);
            t += dt;
        }
        nr_sine_waves++;

        if (freq_step == 0) break;
    }

    for (int i = 0; i < n; i++) {
        //sw[i] *= (scale / nr_sine_waves);
        sw[i] *= scale;
    }
}

#define RAND_0_TO_1       ((double)random() / RAND_MAX)
#define RAND_MINUS_1_TO_1 ((RAND_0_TO_1 - 0.5) * 2)

static void init_using_white_noise(double *wn, int n, double scale)
{
    for (int i = 0; i < n; i++) {
        wn[i] = scale * RAND_MINUS_1_TO_1;
    }
}

static void add_white_noise(double *data, int n, double scale)
{
    for (int i = 0; i < n; i++) {
        data[i] += scale * RAND_MINUS_1_TO_1;
    }
}

static void init_using_wav_file(double *wav, int n, char *filename, double scale)
{
    int ret, num_chan, num_items, sample_rate;
    double *data;

    ret = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0 || num_chan != 1 || sample_rate < 20000 || sample_rate > 25000) {
        FATAL("read_wav_file ret=%d num_chan=%d sample_rate=%d\n", ret, num_chan, sample_rate);
    }

    for (int i = 0; i < n; i++) {
        wav[i] = scale * data[i%num_items];
    }
    
    free(data);
}   

