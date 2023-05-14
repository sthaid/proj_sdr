#include "common.h"

// xxx
// - better way to change freqs fast

//
// defines
//

#define MAX_DATA_CHUNK  131072
#define MAX_DATA        (2*MAX_DATA_CHUNK)

#define DEMOD_AM  0
#define DEMOD_USB 1
#define DEMOD_LSB 2
#define DEMOD_FM  3
#define DEMOD_STR(x) \
    ((x) == DEMOD_AM  ? "AM": \
     (x) == DEMOD_USB ? "USB": \
     (x) == DEMOD_LSB ? "LSB": \
     (x) == DEMOD_FM  ? "FM": \
                        "????")

#define BAND_NONE 0
#define BAND_AM   1
#define BAND_FM   2

//
// typedefs
//

//
// variables
//

static unsigned long Head;
static unsigned long Tail;
static complex       Data[MAX_DATA];

static bool          sim_mode;

static int           tc_freq_ctr;
static int           tc_freq_offset;
static int           tc_demod;
static int           tc_volume;
static int           tc_lpf_am;
static int           tc_lpf_fm;
static int           tc_lpf_ssb;
static int           tc_band;
static int           tc_fft_pause;

//
// prototypes
//

static complex freq_shift(complex x, double f_shift, double t);
static complex lpf(complex x, double f_cut);
static void downsample_and_audio_out(double x);
static void tc_init(void);

static void display_init(void);
static void display_add_fft(complex data, complex data_lpf, double data_demod);
static void *display_thread(void *cx);

static void sdr_test_init(void);
static void sim_test_init(void);

// -----------------  RX TEST  -----------------------------

void *rx_test(void *cx)
{
    complex    data, data_freq_shift, data_lpf;
    double     data_demod;
    complex    tmp, prev=0, product;
    double     t=0;
    double     volume_scale[] = { [DEMOD_AM]=10, [DEMOD_USB]=3, [DEMOD_LSB]=3, [DEMOD_FM]=10 };

    sim_mode = (strcmp(test_name, "rx_sim") == 0);

    tc_init();
    display_init();

    tc_freq_offset = 0;
    tc_volume      = 10;
    tc_lpf_am      = 4000;
    tc_lpf_fm      = 70000;
    tc_lpf_ssb     = 2000;
    tc_fft_pause   = 300000;
    if (sim_mode) {
        tc_demod = DEMOD_FM;
        tc_freq_ctr = 800 * KHZ;
        tc_band = BAND_NONE;
        sim_test_init();
    } else {
        tc_demod = DEMOD_FM;
        tc_freq_ctr = 103.3 * MHZ;
        tc_band = BAND_FM;
        sdr_test_init();
    }

    // loop, covnerting the sdr data to audio out
    while (true) {
        // wait for data to be available
        if (Head == Tail) {
            usleep(1000);
            continue;
        }
        data = Data[Head % MAX_DATA];

        // frequency shift the data; this allows tuning adjustment 
        // without changing the sdr center freq setting
        data_freq_shift = freq_shift(data, -tc_freq_offset, t);

        // demodulate
        switch (tc_demod) {
        case DEMOD_AM: {
            data_lpf = lpf(data_freq_shift, tc_lpf_am);
            data_demod = cabs(data_lpf);
            break; }
        case DEMOD_USB: case DEMOD_LSB: {
            double shift = (tc_demod == DEMOD_USB ? 2000 : -2000);
            data_lpf = lpf(data_freq_shift, tc_lpf_ssb);
            tmp = freq_shift(data_lpf, shift, t);
            data_demod = creal(tmp) + cimag(tmp);
            break; }
        case DEMOD_FM: {
            data_lpf = lpf(data_freq_shift, tc_lpf_fm);
            product = data_lpf * conj(prev);
            prev = data_lpf;
            data_demod = atan2(cimag(product), creal(product));
            break; }
        default:
            FATAL("invalid tc_demod %d\n", tc_demod);
            break;
        }

        // scale the demodulated data volume
        data_demod = data_demod * volume_scale[tc_demod];

        // downsample and output the audio
        downsample_and_audio_out(data_demod);

        // display ffts
        display_add_fft(data, data_lpf, data_demod);

        // update variables for next iteration
        Head++;
        t += DELTA_T;
    }

    return NULL;
}

static complex freq_shift(complex x, double f_shift, double t)
{
    if (f_shift == 0) {
        return x;
    }

    return x * cexp(I * (TWO_PI * f_shift) * t);
}

static complex lpf(complex x, double f_cut)
{
    static double curr_f_cut;
    static BWLowPass *bwi, *bwq;

    if (f_cut != curr_f_cut) {
        curr_f_cut = f_cut;
        if (bwi) free_bw_low_pass(bwi);
        if (bwq) free_bw_low_pass(bwq);
        bwi = create_bw_low_pass_filter(LPF_ORDER, SAMPLE_RATE, f_cut); 
        bwq = create_bw_low_pass_filter(LPF_ORDER, SAMPLE_RATE, f_cut);
    }

    return bw_low_pass(bwi, creal(x)) + bw_low_pass(bwq, cimag(x)) * I;
}

static void downsample_and_audio_out(double x)
{
    static int cnt;
    static void *ma_cx;
    double ma;

    #define NUM_DS ((int)(0.97 * SAMPLE_RATE / AUDIO_SAMPLE_RATE))

    ma = moving_avg(x, NUM_DS, &ma_cx); 

    if (cnt++ == NUM_DS) {
        audio_out(ma * (tc_volume / 100.));
        cnt = 0;
    }
}

static void tc_init(void)
{
    tc.ctrl[0] = (struct test_ctrl_s)
                 {"F", &tc_freq_ctr, 0, 200*MHZ, 10*KHZ,   // 0 to 200 MHx
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW, SDL_EVENT_KEY_RIGHT_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {"F_OFF", &tc_freq_offset, -600*KHZ, 600*KHZ, 100,
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW+CTRL, SDL_EVENT_KEY_RIGHT_ARROW+CTRL};
    tc.ctrl[2] = (struct test_ctrl_s)
                 {"VOLUME", &tc_volume, 0, 100, 1,
                  {}, NULL,
                  SDL_EVENT_KEY_DOWN_ARROW, SDL_EVENT_KEY_UP_ARROW};
    tc.ctrl[3] = (struct test_ctrl_s)
                 {"DEMOD", &tc_demod, 0, 3, 1,
                  {"AM", "USB", "LSB", "FM"}, NULL, 
                  '<', '>'};
    tc.ctrl[4] = (struct test_ctrl_s)
                 {"LPF_AM", &tc_lpf_am, 1*KHZ, 100*KHZ, 100,
                  {}, "HZ", 
                  '1', '2'};
    tc.ctrl[5] = (struct test_ctrl_s)
                 {"LPF_FM", &tc_lpf_fm, 1*KHZ, 100*KHZ, 100,
                  {}, "HZ", 
                  '3', '4'};
    tc.ctrl[6] = (struct test_ctrl_s)
                 {"LPF_SSB", &tc_lpf_ssb, 1*KHZ, 100*KHZ, 100,
                  {}, "HZ", 
                  '5', '6'};

    tc.ctrl[7] = (struct test_ctrl_s)
                 {"F", &tc_freq_ctr, 0, 200*MHZ, 100*KHZ,
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW+ALT, SDL_EVENT_KEY_RIGHT_ARROW+ALT};
    tc.ctrl[8] = (struct test_ctrl_s)
                 {"BAND", &tc_band, BAND_NONE, BAND_FM, 1,
                  {"NONE", "AM", "FM"}, NULL,
                  '[', ']'};
    tc.ctrl[13] = (struct test_ctrl_s)
                 {"FFT_PAUSE", &tc_fft_pause, 0, 1000000, 10000,
                  {}, NULL,
                  'p', 'P'};
}

// -----------------  RX DISPLAY  --------------------------

#define FFT_N (SAMPLE_RATE / 10)

static struct {
    complex *data;
    complex *data_lpf;
    double  *data_demod;
    int      n;
    complex *data_fft;
    complex *data_lpf_fft;
    complex *data_demod_fft;
    int  pause;
    bool updated;
} fft;

static void display_init(void)
{
    pthread_t tid;

    fft.data           = fftw_alloc_complex(FFT_N);
    fft.data_fft       = fftw_alloc_complex(FFT_N);
    fft.data_lpf       = fftw_alloc_complex(FFT_N);
    fft.data_lpf_fft   = fftw_alloc_complex(FFT_N);
    fft.data_demod     = fftw_alloc_real(FFT_N);
    fft.data_demod_fft = fftw_alloc_complex(FFT_N);

    pthread_create(&tid, NULL, display_thread, NULL);
}

static void display_add_fft(complex data, complex data_lpf, double data_demod)
{
    if (fft.pause > 0) {
        fft.pause--;
        fft.n = 0;
        return;
    }

    if (fft.n < FFT_N) {
        fft.data[fft.n] = data;
        fft.data_lpf[fft.n] = data_lpf;
        fft.data_demod[fft.n] = data_demod;
        if (fft.n == FFT_N-1) {
            __sync_synchronize();
        }
        fft.n++;
    }
}

static void *display_thread(void *cx)
{
    double yv_max, range;
    int fft_n;

    while (true) {
        // display info
        sprintf(tc.info, "FREQ = %0.6f MHz  DEMOD = %s",
                (double)(tc_freq_ctr + tc_freq_offset) / MHZ,
                DEMOD_STR(tc_demod));

        // if fft data set is available then calculate and plot the ffts
        fft_n = fft.n;
        if (fft_n == FFT_N) {
            yv_max = (sim_mode ? 150000 
                               : 4000);
            range  = SAMPLE_RATE/4;
            fft_fwd_c2c(fft.data, fft.data_fft, fft_n);
            plot_fft(0, fft.data_fft, fft_n, SAMPLE_RATE, 
                     -range, range, yv_max, tc_freq_offset, NOC, "DATA_FFT", 
                     0, 0, 100, 25);

            yv_max =  (sim_mode ? (tc_demod == DEMOD_FM ? 200000 : 30000) 
                                : (4000));
            range  = (tc_demod == DEMOD_FM ? 100*KHZ : 10*KHZ);
            fft_fwd_c2c(fft.data_lpf, fft.data_lpf_fft, fft_n);
            plot_fft(1, fft.data_lpf_fft, fft_n, SAMPLE_RATE, 
                     -range, range, yv_max, 0, NOC, "DATA_LPF_FFT", 
                     0, 25, 100, 25);

            yv_max = 30000;
            range  = 10*KHZ;
            fft_fwd_r2c(fft.data_demod, fft.data_demod_fft, fft_n);
            plot_fft(2, fft.data_demod_fft, fft_n, SAMPLE_RATE, 
                     -range, range, yv_max, 0, NOC, "DATA_DEMOD_FFT", 
                     0, 50, 100, 25);

            fft.n = 0;
            fft.updated = true;
        }

        // delay 10 ms
        usleep(10000);
    }

    return NULL;
}

// -----------------  RX SIMULATOR  ------------------------

static void *sim_thread(void *cx);

static void sim_test_init(void)
{
    pthread_t tid;

    pthread_create(&tid, NULL, sim_thread, NULL);
}

static void *sim_thread(void *cx)
{
    int fd;
    struct stat statbuf;
    size_t file_offset, file_size, len;
    double t=0, antenna[MAX_DATA_CHUNK], w;
    complex *data;

    // open ANTENNA_FILENAME
    fd = open(ANTENNA_FILENAME, O_RDONLY);
    if (fd < 0) {
        FATAL("failed open %s, %s\n", ANTENNA_FILENAME, strerror(errno));
    }

    // get size of ANTENNA_FILENAME
    fstat(fd, &statbuf);
    file_size = statbuf.st_size;
    file_offset = 0;

    // loop forever, 
    // when end of file is reached start over from file begining
    while (true) {
        // wait for space to be available in Data array, 
        while (MAX_DATA - (Tail - Head) < MAX_DATA_CHUNK) {
            usleep(1000);
        }

        // read values from antenna file, these are real values
        len = pread(fd, antenna, MAX_DATA_CHUNK*sizeof(double), file_offset);
        if (len != MAX_DATA_CHUNK*sizeof(double)) {
            FATAL("read %s, len=%ld, %s\n", ANTENNA_FILENAME, len, strerror(errno));
        }
        file_offset += len;
        if (file_offset + len > file_size) {
            file_offset = 0;
        }

        // xxx use fft.pause here too

        // frequency shift the antenna data, and 
        // store result in Data[Tail], these are complex values
        data = &Data[Tail % MAX_DATA];
        w = TWO_PI * tc_freq_ctr;
        for (int i = 0; i < MAX_DATA_CHUNK; i++) {
            data[i] = antenna[i] * cexp(-I * w * t);
            t += DELTA_T;
        }

        // increment Tail
        Tail += MAX_DATA_CHUNK;
    }

    return NULL;
}

// -----------------  RX SDR INTFC  ------------------------

static void *sdr_ctrl_thread(void *cx);
static void sdr_cb(unsigned char *iq, size_t len);

static void sdr_test_init(void)
{
    pthread_t tid;

    pthread_create(&tid, NULL, sdr_ctrl_thread, NULL);
}

static void * sdr_ctrl_thread(void *cx)
{
    int last_band = tc_band;
    int last_freq_ctr = tc_freq_ctr;

    sdr_init(tc_freq_ctr, sdr_cb);

    while (true) {
        if (tc_freq_ctr != last_freq_ctr && fft.updated) {
            if (tc_fft_pause > 0) {
                fft.pause = tc_fft_pause;
                fft.updated = false;
            }
            sdr_set_freq(tc_freq_ctr);
            last_freq_ctr = tc_freq_ctr;
        }

        if (last_band != tc_band) {
            switch (tc_band) {
            case BAND_AM:
                tc_freq_ctr = 1.030 * MHZ;
                tc_freq_offset = 0;
                tc_demod = DEMOD_AM;
                break;
            case BAND_FM:
                tc_freq_ctr = 103.3 * MHZ;
                tc_freq_offset = 0;
                tc_demod = DEMOD_FM;
                break;
            }
            sdr_set_freq(tc_freq_ctr);
            last_band = tc_band;
            last_freq_ctr = tc_freq_ctr;
        }

        // xxx add more sdr controls, such as gain

        usleep(10000);
    }

    return NULL;
}

static void sdr_cb(unsigned char *iq, size_t len)
{
    int items=len/2, i, j;

    if (MAX_DATA - (Tail - Head) < items) {
        NOTICE("discarding sdr data\n");
        return;
    }

    j = Tail % MAX_DATA;
    for (i = 0; i < items; i++) {
        Data[j++] = ((iq[2*i+0] - 128.) + (iq[2*i+1] - 128.) * I) / 128.;
        if (j == MAX_DATA) j = 0;
    }

    Tail += items;
}
