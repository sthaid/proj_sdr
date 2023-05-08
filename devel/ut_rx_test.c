#include "common.h"

// xxx add audio fft and audio time dom

// defines

#define MAX_DATA_CHUNK  131072
#define MAX_DATA        (4*MAX_DATA_CHUNK)

#define DEMOD_AM  0
#define DEMOD_USB 1
#define DEMOD_LSB 2
#define DEMOD_FM  3
#define DEMOD_STR(x) \
    ((x) == DEMOD_AM ? "AM": \
     (x) == DEMOD_USB ? "USB": \
     (x) == DEMOD_LSB ? "LSB": \
     (x) == DEMOD_FM ? "FM": \
                       "????")

// typedefs

typedef struct {
    complex *data;
    complex *data_fft;
    complex *data_lpf;
    complex *data_lpf_fft;
    double  *data_demod;
    complex *data_demod_fft;
    int      n;
} fft_t;

// variables

static unsigned long Head;
static unsigned long Tail;
static complex       Data[MAX_DATA];

static fft_t         fft;

static double        tc_freq;
static double        tc_freq_offset;
static double        tc_demod;
static double        tc_volume;
static double        tc_lpf_am;
static double        tc_lpf_fm;
static double        tc_lpf_ssb;
static double        tc_k1;
//static double        tc_k2;
//static double        tc_k3;

// prototypes

//xxx names
static complex freq_shift(complex x, double f_shift, double t);
static complex lpf(complex x, double f_cut);
static void downsample_and_audio_out(double x);

static void rx_tc_init(void);

static void rx_display_init(void);
static void rx_display_add_fft(complex data, complex data_lpf, double data_demod);
static void *rx_display_thread(void *cx);

static void rx_sdr_init(void);
static void rx_sim_init(void);

// -----------------  RX TEST  -----------------------------

void *rx_test(void *cx)
{
    complex    data, data_freq_shift, data_lpf;
    double     data_demod;
    complex    tmp, prev=0, product;
    double     t=0;
    bool       sim_mode = (strcmp(test_name, "rx_sim") == 0);

    rx_tc_init();
    rx_display_init();

    tc_freq_offset = 0;
    tc_volume      = .1;
    tc_lpf_am      = 4000;
    tc_lpf_fm      = 60000;
    tc_lpf_ssb     = 2000;

    if (sim_mode) {
        tc_demod = DEMOD_FM;
        tc_freq = 700 * KHZ;
        //tc_demod = DEMOD_AM;
        //tc_freq = 500 * KHZ;
        rx_sim_init();
    } else {
        tc_demod = DEMOD_FM;
        tc_freq = 100.7 * MHZ;
        rx_sdr_init();
    }

    while (true) {
        if (Head == Tail) {
            usleep(1000);
            continue;
        }

        data = Data[Head % MAX_DATA];

        data_freq_shift = freq_shift(data, -tc_freq_offset, t);

        switch ((int)tc_demod) {
        case DEMOD_AM: {
            data_lpf = lpf(data_freq_shift, tc_lpf_am);
            data_demod = cabs(data_lpf);
            data_demod *= 10;  // xxx
            break; }
        case DEMOD_USB: {
            data_lpf = lpf(data_freq_shift, tc_lpf_ssb);
            tmp = freq_shift(data_lpf, 2000, t);
            data_demod = creal(tmp) + cimag(tmp);  // xxx or cabs?
            break; }
        case DEMOD_LSB: {
            data_lpf = lpf(data_freq_shift, tc_lpf_ssb);
            tmp = freq_shift(data_lpf, -2000, t);
            data_demod = creal(tmp) + cimag(tmp);  // xxx or cabs?
            break; }
        case DEMOD_FM: {
            data_lpf = lpf(data_freq_shift, tc_lpf_fm);
            product = data_lpf * conj(prev);
            prev = data_lpf;
            data_demod = atan2(cimag(product), creal(product));
            data_demod *= 10;  // xxx
            break; }
        default:
            FATAL("invalid demod %f\n", tc_demod);
            break;
        }

        downsample_and_audio_out(data_demod);

        //xxx 
        rx_display_add_fft(data, data_lpf, data_demod);

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
        bwi = create_bw_low_pass_filter(20, SAMPLE_RATE, f_cut); 
        bwq = create_bw_low_pass_filter(20, SAMPLE_RATE, f_cut);
    }

    return bw_low_pass(bwi, creal(x)) + bw_low_pass(bwq, cimag(x)) * I;
}

static void downsample_and_audio_out(double x)
{
    static int cnt;
    static void *ma_cx;
    double ma;

    ma = moving_avg(x, 1000, &ma_cx); 

    if (cnt++ == (SAMPLE_RATE / AUDIO_SAMPLE_RATE)) {
        audio_out(ma * tc_volume);  // xxx auto volume scale
        cnt = 0;
    }
}

// -----------------  RX TEST CONTROL  ---------------------

static void rx_tc_init(void)
{
    tc.ctrl[0] = (struct test_ctrl_s)
                 {"F", &tc_freq, 0, 200000000, 10000,   // 0 to 200 MHx
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW, SDL_EVENT_KEY_RIGHT_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {"F_OFF", &tc_freq_offset, -600000, 600000, 100,
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW+CTRL, SDL_EVENT_KEY_RIGHT_ARROW+CTRL};
    tc.ctrl[2] = (struct test_ctrl_s)
                 {"VOLUME", &tc_volume, 0.01, 2.00, 0.01,
                  {}, NULL,
                  SDL_EVENT_KEY_DOWN_ARROW, SDL_EVENT_KEY_UP_ARROW};
    tc.ctrl[3] = (struct test_ctrl_s)
                 {"DEMOD", &tc_demod, 0, 3, 1,
                  {"AM", "USB", "LSB", "FM"}, NULL, 
                  '<', '>'};
    tc.ctrl[4] = (struct test_ctrl_s)
                 {"LPF_AM", &tc_lpf_am, 1000, 500000, 100,
                  {}, "HZ", 
                  '1', '2'};
    tc.ctrl[5] = (struct test_ctrl_s)
                 {"LPF_FM", &tc_lpf_fm, 1000, 500000, 100,
                  {}, "HZ", 
                  '3', '4'};
    tc.ctrl[6] = (struct test_ctrl_s)
                 {"LPF_SSB", &tc_lpf_ssb, 1000, 500000, 100,
                  {}, "HZ", 
                  '5', '6'};

    tc.ctrl[7] = (struct test_ctrl_s)
                 {"K1", &tc_k1, 0, 1, .1,
                  {}, NULL,
                  'f', 'F'};
#if 0
    tc.ctrl[8] = (struct test_ctrl_s)
                 {"K2", &tc_k2, 0, 1, .1,
                  {}, NULL,
                  '3', '4'};
    tc.ctrl[9] = (struct test_ctrl_s)
                 {"K3", &tc_k3, 0, 1, .1,
                  {}, NULL,
                  '5', '6'};
#endif
}

// -----------------  RX DISPLAY  --------------------------

// xxx display section

#define FFT_N           240000
#define FFT_INTERVAL_US 100000   // .1 sec

static void rx_display_init(void)
{
    pthread_t tid;

    fft.data           = fftw_alloc_complex(FFT_N);
    fft.data_fft       = fftw_alloc_complex(FFT_N);
    fft.data_lpf       = fftw_alloc_complex(FFT_N);
    fft.data_lpf_fft   = fftw_alloc_complex(FFT_N);
    fft.data_demod     = fftw_alloc_real(FFT_N);
    fft.data_demod_fft = fftw_alloc_complex(FFT_N);

    pthread_create(&tid, NULL, rx_display_thread, NULL);
}

static void rx_display_add_fft(complex data, complex data_lpf, double data_demod)
{
    if (fft.n < FFT_N) {
        fft.data[fft.n] = data;
        fft.data_lpf[fft.n] = data_lpf;
        fft.data_demod[fft.n] = data_demod;
        fft.n++;
    }
}

static void *rx_display_thread(void *cx)
{
    double               yv_max0, yv_max1, yv_max2;
    double               range0, range1, range2;
    unsigned long        tnow;
    static unsigned long tlast;

    while (true) {
        yv_max0 = 150000;
        yv_max1 =  (tc_demod == DEMOD_FM ? 200000 : 30000);
        yv_max2 =  30000;

        range0 = SAMPLE_RATE/2;
        range1 = (tc_demod == DEMOD_FM ? 100*KHZ : 10*KHZ);
        range2 = 10*KHZ;

        tnow = microsec_timer();
        if (fft.n != FFT_N || tnow-tlast < FFT_INTERVAL_US) {
            usleep(10000);
            continue;
        }

        sprintf(tc.info, "FREQ = %0.3f MHz  DEMOD = %s",
                (tc_freq + tc_freq_offset) / MHZ,
                DEMOD_STR(tc_demod));

        fft_fwd_c2c(fft.data, fft.data_fft, fft.n);
        plot_fft(0, fft.data_fft, fft.n, SAMPLE_RATE, 
                 -range0, range0, yv_max0, tc_freq_offset, NOC, "DATA_FFT", 
                 0, 0, 100, 25);

        fft_fwd_c2c(fft.data_lpf, fft.data_lpf_fft, fft.n);
        plot_fft(1, fft.data_lpf_fft, fft.n, SAMPLE_RATE, 
                 -range1, range1, yv_max1, 0, NOC, "DATA_LPF_FFT", 
                 0, 25, 100, 25);

        fft_fwd_r2c(fft.data_demod, fft.data_demod_fft, fft.n);
        plot_fft(2, fft.data_demod_fft, fft.n, SAMPLE_RATE, 
                 -range2, range2, yv_max2, 0, NOC, "DATA_DEMOD_FFT", 
                 0, 50, 100, 25);

        // xxx add audio fft

        tlast = tnow;
        fft.n = 0;
    }

    return NULL;
}

// -----------------  RX SIMULATOR  ------------------------

static void *rx_sim_thread(void *cx);

static void rx_sim_init(void)
{
    pthread_t tid;

    pthread_create(&tid, NULL, rx_sim_thread, NULL);
}

static void *rx_sim_thread(void *cx)
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

        // frequency shift the antenna data, and 
        // store result in Data[Tail], these are complex values
        data = &Data[Tail % MAX_DATA];
        w = TWO_PI * tc_freq;
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

static void *rx_sdr_ctrl_thread(void *cx);
static void rx_sdr_cb(unsigned char *iq, size_t len);

static void rx_sdr_init(void)
{
    pthread_t tid;

    pthread_create(&tid, NULL, rx_sdr_ctrl_thread, NULL);
}

static void * rx_sdr_ctrl_thread(void *cx)
{
    double curr_freq;

    curr_freq = tc_freq;

    sdr_init(curr_freq, rx_sdr_cb);

    while (true) {
        if (curr_freq != tc_freq) {
            NOTICE("SETTING FREQ %f\n", tc_freq);
            sdr_set_freq(tc_freq);
            curr_freq = tc_freq;
        }

        // xxx add more sdr controls, such as gain

        usleep(10000);
    }

    return NULL;
}

static void rx_sdr_cb(unsigned char *iq, size_t len)
{
    int items=len/2, i, j;

    if (MAX_DATA - (Tail - Head) < items) {
        NOTICE("discarding sdr data\n");
        return;
    }

    j = Tail % MAX_DATA;
    for (i = 0; i < items; i++) {
        Data[j++] = ((iq[2*i+0] - 128.) + (iq[2*i+1] - 128.) * I) / 128.;  // xxx try without all the 128.
        if (j == MAX_DATA) j = 0;
    }

    Tail += items;
}
