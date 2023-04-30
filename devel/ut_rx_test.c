#include "common.h"

// defines

#define MAX_DATA_CHUNK  131072
#define MAX_DATA        (4*MAX_DATA_CHUNK)

#define DEMOD_AM 0
#define DEMOD_FM 1
#define DEMOD_STR(x) \
    ((x) == DEMOD_AM ? "AM": \
     (x) == DEMOD_FM ? "FM": \
                       "????")

// typedefs

typedef struct {
    complex *data;
    complex *data_fft;
    complex *data_lpf;
    complex *data_lpf_fft;
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
static double        tc_lpf_cutoff;
static double        tc_volume;
static double        tc_reset;
static double        tc_k1;
static double        tc_k2;
static double        tc_k3;

// prototypes

static void rx_tc_init(void);
static void rx_tc_reset(void);

static void rx_fft_init(void);
static void rx_fft_add_data(complex data, complex data_lpf);
static void *rx_fft_thread(void *cx);

static void rx_demod_am(complex data_lpf);
static void rx_demod_fm(complex data_lpf);

static void rx_sdr_init(void);
static void rx_sim_init(void);

// xxx
// - exapand the graph to just part of the fft

// -----------------  RX TEST  -----------------------------

void *rx_test(void *cx)
{
    pthread_t  tid;
    BWLowPass *bwi=NULL, *bwq=NULL;
    complex    data_orig, data, data_lpf;
    double     t=0;
    int cnt=0;
    double curr_lpf_cutoff = 0;

    rx_tc_init();
    rx_fft_init();

    if (strcmp(test_name, "rx_sim") == 0) {
        rx_sim_init();
    } else {
        rx_sdr_init();
    }

    pthread_create(&tid, NULL, rx_fft_thread, NULL);

    while (true) {
        if (tc_reset) {
            rx_tc_reset();
        }

        if (curr_lpf_cutoff != tc_lpf_cutoff) {
            if (bwi) free_bw_low_pass(bwi);
            if (bwq) free_bw_low_pass(bwq);
            bwi = create_bw_low_pass_filter(8, SAMPLE_RATE, tc_lpf_cutoff); 
            bwq = create_bw_low_pass_filter(8, SAMPLE_RATE, tc_lpf_cutoff);
            curr_lpf_cutoff = tc_lpf_cutoff;
        }

        if (cnt++ >= SAMPLE_RATE/10) {
            sprintf(tc.info, "FREQ = %0.3f MHz  DEMOD = %s",
                    (tc_freq + tc_freq_offset) / MHZ,
                    DEMOD_STR(tc_demod));
            cnt = 0;
        }

        if (Head == Tail) {
            usleep(1000);
            continue;
        }

        data_orig = Data[Head % MAX_DATA];

        if (tc_freq_offset) {
            double w = TWO_PI * tc_freq_offset;
            data = data_orig * cexp(-I * w * t);
        } else {
            data = data_orig;
        }

        data_lpf = bw_low_pass(bwi, creal(data)) +
                   bw_low_pass(bwq, cimag(data)) * I;

        rx_fft_add_data(data_orig, data_lpf);

        switch ((int)tc_demod) {
        case DEMOD_AM:
            rx_demod_am(data_lpf);
            break;
        case DEMOD_FM:
            rx_demod_fm(data_lpf);
            break;
        default:
            FATAL("invalid demod %f\n", tc_demod);
            break;
        }

        Head++;

        t += DELTA_T;
    }

    return NULL;
}

// -----------------  RX TEST CONTROL  ---------------------

static void rx_tc_init(void)
{
    tc.ctrl[0] = (struct test_ctrl_s)
                 {"F", &tc_freq, 0, 200000000, 10000,   // 0 to 200 MHx
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW, SDL_EVENT_KEY_RIGHT_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {"F_OFF", &tc_freq_offset, -600000, 600000, 1000,
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW+CTRL, SDL_EVENT_KEY_RIGHT_ARROW+CTRL};
    tc.ctrl[2] = (struct test_ctrl_s)
                 {"LPF_CUT", &tc_lpf_cutoff, 1000, 500000, 1000,
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW+ALT, SDL_EVENT_KEY_RIGHT_ARROW+ALT};
    tc.ctrl[3] = (struct test_ctrl_s)
                 {"VOLUME", &tc_volume, 0, 100, 1,
                  {}, NULL,
                  SDL_EVENT_KEY_DOWN_ARROW, SDL_EVENT_KEY_UP_ARROW};
    tc.ctrl[4] = (struct test_ctrl_s)
                 {"DEMOD", &tc_demod, 0, 1, 1,
                  {"AM", "FM"}, NULL, 
                  '<', '>'};
    tc.ctrl[5] = (struct test_ctrl_s)
                 {"RESET", &tc_reset, 0, 1, 1,
                  {"", ""}, NULL, 
                  SDL_EVENT_NONE, 'r'};

    tc.ctrl[7] = (struct test_ctrl_s)
                 {"K1", &tc_k1, 0.001, 0.100, .001,
                  {}, NULL,
                  '1', '2'};
    tc.ctrl[8] = (struct test_ctrl_s)
                 {"K2", &tc_k2, 10, 5000, 10,    
                  {}, NULL,
                  '3', '4'};
    tc.ctrl[9] = (struct test_ctrl_s)
                 {"K3", &tc_k3, 0, 100, 1,
                  {}, NULL,
                  '5', '6'};

    rx_tc_reset();
}

static void rx_tc_reset(void)
{
    if (strcmp(test_name, "rx_sdr") == 0) {
        tc_freq = (tc_demod == DEMOD_AM ? 1030*KHZ : 100.7*MHZ);
    } else {
        tc_freq = (tc_demod == DEMOD_AM ? 500*KHZ : 800*KHZ);
    }
    tc_freq_offset  = 0;
    tc_lpf_cutoff   = (tc_demod == DEMOD_AM ? 4000 : 60000);
    tc_volume       = 10;
    tc_reset        = 0;
    tc_k1           = 0.004;
    tc_k2           = 1000;
    tc_k3           = 0;
}

// -----------------  RX FFT  ------------------------------

#define FFT_N           240000
#define FFT_INTERVAL_US 100000   // .1 sec

static void rx_fft_init(void)
{
    fft.data         = fftw_alloc_complex(FFT_N);
    fft.data_fft     = fftw_alloc_complex(FFT_N);
    fft.data_lpf     = fftw_alloc_complex(FFT_N);
    fft.data_lpf_fft = fftw_alloc_complex(FFT_N);
}

static void rx_fft_add_data(complex data, complex data_lpf)
{
    if (fft.n < FFT_N) {
        fft.data[fft.n] = data;
        fft.data_lpf[fft.n] = data_lpf;
        fft.n++;
    }
}

static void *rx_fft_thread(void *cx)
{
    double               yv_max;
    unsigned long        tnow;
    static unsigned long tlast;

    if (strcmp(test_name, "rx_sim") == 0) {
        yv_max = 150000;
    } else {
        yv_max = 4000;
    }

    while (true) {
        tnow = microsec_timer();
        if (fft.n != FFT_N || tnow-tlast < FFT_INTERVAL_US) {
            usleep(10000);
            continue;
        }

        fft_fwd_c2c(fft.data, fft.data_fft, fft.n);
        plot_fft(0, fft.data_fft, fft.n, SAMPLE_RATE, false, yv_max, tc_freq_offset, "DATA_FFT", 0, 0, 100, 30);

        // xxx expand the plot?
        fft_fwd_c2c(fft.data_lpf, fft.data_lpf_fft, fft.n);
        plot_fft(1, fft.data_lpf_fft, fft.n, SAMPLE_RATE, false, yv_max, 0, "DATA_LPF_FFT", 0, 30, 100, 30);
        // xxx                                                           ^

        tlast = tnow;
        fft.n = 0;
    }

    return NULL;
}

// -----------------  RX DEMODULATORS  ---------------------

static void rx_demod_am(complex data_lpf)
{
    double        yo;
    static int    cnt;
    static void *ma_cx;
    static int current;

    // xxx improve the AM detector

    yo = cabs(data_lpf);
    if (tc_k2 != current) {
        NOTICE("XXX %f\n", tc_k2);
        ma_cx = NULL;
        current = tc_k2;
    }
    yo = moving_avg(yo, current, &ma_cx); 

    // xxx why 0.97
    if (cnt++ == (int)(0.95 * SAMPLE_RATE / 22000)) {  // xxx 22000 is the aplay rate
        audio_out(yo*tc_volume);  // xxx auto scale
        cnt = 0;
    }
}

static void rx_demod_fm(complex data_lpf)
{
    static complex prev;
    double yo;
    static int cnt;
    complex product;
    static void *ma_cx=NULL;

    product = data_lpf * conj(prev);
    yo = atan2(cimag(product), creal(product));
    prev = data_lpf;

    yo = moving_avg(yo, 110, &ma_cx);

    // xxx why 0.97
    if (cnt++ == (int)(0.97 * SAMPLE_RATE / 22000)) {  // xxx 22000 is the aplay rate
        audio_out(yo*tc_volume/10.);
        cnt = 0;
    }
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
    double t, antenna[MAX_DATA_CHUNK], w;
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

    // other inits
    t = 0;

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
        Data[j] = ((iq[2*i+0] - 128.) + (iq[2*i+1] - 128.) * I) / 128.;  // xxx try without all the 128.
        if (++j == MAX_DATA) j = 0;
    }

    Tail += items;
}
