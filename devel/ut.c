// xxx comments
// xxx put tick marks on plot
// xxx better way to handle global args
// xxx audio out
// xxx add gen.c
// xxx init_fft?
// xxx move plot y axis to left by 1 pixel
// xxx exit pgm

#include "common.h"

//
// defines
//

#define MAX_TESTS (sizeof(tests)/sizeof(tests[0]))

// xxx yv min/max for complex
//    maybe new PLOT_COMPLEX
#define PLOT(_idx,_is_complex,_data,_n,_xvmin,_xvmax,_yvmin,_yvmax,_title) \
    do { \
        plots_t *p = &plots[_idx]; \
        pthread_mutex_lock(&mutex); \
        if (!(_is_complex)) { \
            memcpy(p->pd, _data, (_n)*sizeof(double)); \
        } else { \
            complex *data_complex = (complex*)(_data); \
            for (int i = 0; i < (_n); i++) { \
                p->pd[i] = cabs(data_complex[i]); \
            } \
            normalize(p->pd, _n, 0, 1); \
        } \
        p->n       = _n; \
        p->xv_min  = _xvmin; \
        p->xv_max  = _xvmax; \
        p->yv_min  = _yvmin; \
        p->yv_max  = _yvmax; \
        p->title   = _title; \
        pthread_mutex_unlock(&mutex); \
    } while (0)

//
// typedefs
//

typedef struct {
    double  pd[250000];  //xxx make sure this is enough
    int     n;
    double  xv_min;
    double  xv_max;
    double  yv_min;
    double  yv_max;
    char   *title;
} plots_t;

typedef struct {
    char  title[100];
    char *bool_name;
    bool  bool_val;
    char *int_name;
    int   int_val;
    int   int_step;
    int   int_min;
    int   int_max;
} test_ctrl_t;

//
// variables
//

char *progname = "ut";

char *arg1_str, *arg2_str, *arg3_str;
int   arg1=-1, arg2=-1, arg3=-1;

test_ctrl_t tc;

plots_t plots[MAX_PLOT];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//
// prototypes
//

void usage(void);
int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

void *plot_test(void *cx);
void *lpf_test(void *cx);
void *audio_test(void *cx);
void *gen_test(void *cx);
void *rx_test(void *cx);

//
// test table
//

static struct test_s {
    char *test_name;
    void *(*proc)(void *cx);
} tests[] = {
        { "plot",   plot_test  },
        { "lpf",    lpf_test   },
        { "audio",  audio_test },
        { "gen",    gen_test   },
        { "rx",     rx_test    },
                };

// -----------------  MAIN  --------------------------------

int main(int argc, char **argv)
{
    int i;
    struct test_s *t;
    pthread_t tid;
    char *test_name;

    init_fft();  // xxx is this needed, elim later

    if (argc == 1) {
        usage();
        return 1;
    }
    test_name = argv[1];
    if (argc > 2) { arg1_str = argv[2]; sscanf(arg1_str, "%d", &arg1); }
    if (argc > 3) { arg2_str = argv[3]; sscanf(arg2_str, "%d", &arg2); }
    if (argc > 4) { arg3_str = argv[4]; sscanf(arg3_str, "%d", &arg3); }
    
    for (i = 0; i < MAX_TESTS; i++) {
        t = &tests[i];
        if (strcmp(test_name, t->test_name) == 0) {
            break;
        }
    }
    if (i == MAX_TESTS) {
        FATAL("test '%s' not found\n", test_name);
    }

    NOTICE("Running '%s' test\n", t->test_name);
    strcpy(tc.title, test_name);
    pthread_create(&tid, NULL, t->proc, NULL);

    // init sdl
    static int win_width = 1600;
    static int win_height = 800;
    if (sdl_init(&win_width, &win_height, false, false, false) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
        return 1;
    }
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        100000,          // 0=continuous, -1=never, else us
        1,              // number of pane handler varargs that follow
        pane_hndlr, NULL, 0, 0, win_width, win_height, PANE_BORDER_STYLE_NONE);
}

void usage(void)
{
    int i;
    struct test_s *t;

    NOTICE("tests:\n");
    for (i = 0; i < MAX_TESTS; i++) {
        t = &tests[i];
        NOTICE("  %s\n", t->test_name);
    }
}

int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;
    char str[200], *p;

    #define FONTSZ 20

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < MAX_PLOT; i++) {
            plots_t *p = &plots[i];
            if (p->title) {
                sdl_plot(pane, i,
                        p->pd, p->n,
                        p->xv_min, p->xv_max,
                        p->yv_min, p->yv_max,
                        p->title);
            }
        }
        pthread_mutex_unlock(&mutex);  // sep mutex for each xxx

        p = str;
        p += sprintf(p, "%s  ", tc.title);
        if (tc.bool_name) {
            p += sprintf(p, "%s=%s  ", tc.bool_name, tc.bool_val ? "TRUE" : "FALSE");
        }
        if (tc.int_name) {
            p += sprintf(p, "%s=%d  ", tc.int_name, tc.int_val);
        }
        sdl_render_printf(pane, 
                          0, pane->h-ROW2Y(1,FONTSZ), FONTSZ,
                          SDL_WHITE, SDL_BLACK, "%s" , str);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_KEY_LEFT_ARROW:
        case SDL_EVENT_KEY_RIGHT_ARROW: {
            int tmp;
            if (tc.int_name == NULL) break;
            tmp = tc.int_val + 
                  (event->event_id == SDL_EVENT_KEY_LEFT_ARROW ? -tc.int_step : tc.int_step);
            if (tmp < tc.int_min) tmp = tc.int_min;
            if (tmp > tc.int_max) tmp = tc.int_max;
            tc.int_val = tmp;
            break; }
        case ' ':
            if (tc.bool_name == NULL) break;
            tc.bool_val = !tc.bool_val;
            break;
        default:
            break;
        }

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ---------------------------
    // -------- TERMINATE --------
    // ---------------------------

    if (request == PANE_HANDLER_REQ_TERMINATE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // not reached
    printf("FATAL: not reached\n");
    exit(1);
    return PANE_HANDLER_RET_NO_ACTION;
}

// -----------------  UTILS  -------------------------------

void add_sine_wave(int sample_rate, int f, int n, double *data)
{
    double  w = TWO_PI * f;
    double  t = 0;
    double  dt = (1. / sample_rate);
    int     i;

    for (i = 0; i < n; i++) {
        data[i] += sin(w * t);
        t += dt;
    }
}

void zero_real(double *data, int n)
{
    memset(data, 0, n*sizeof(double));
}

void zero_complex(complex *data, int n)
{
    memset(data, 0, n*sizeof(complex));
}

// xxx support multiple chan
void read_and_filter_wav_file(char *filename, double **data_arg, int *num_items_arg, int *sample_rate_arg, int lpf_freq)
{
    int ret;
    double *data;
    int num_items, sample_rate, num_chan;

    ret = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0 || num_chan != 1) {
        FATAL("read_wav_file ret=%d num_chan=%d\n", ret, num_chan);
    }
    NOTICE("read_wav_file num_chan=%d num_items=%d sample_rate=%d\n", num_chan, num_items, sample_rate);

    fft_lpf_real(data, data, num_items, sample_rate, lpf_freq);
    normalize(data, num_items, -1, 1);

    *data_arg        = data;
    *num_items_arg   = num_items;
    *sample_rate_arg = sample_rate;
}

// -----------------  PLOT TEST  ---------------------------

void *plot_test(void *cx)
{
    int sample_rate, f, n;
    double *data;

    #define SECS 1

    sample_rate = 24000;
    f = 10;
    n = SECS * sample_rate;
    data = fftw_alloc_real(n);
    zero_real(data, n);

    add_sine_wave(sample_rate, f, n, data);

    PLOT(0, false, data, n,  0, 1,  -1, +1,  "SINE WAVE");
    PLOT(1, false, data, n,  0, 1,  -0.5, +0.5,  "SINE WAVE");
    PLOT(2, false, data, n,  0, 1,  0.5, +1.5,  "SINE WAVE");

    return NULL;
}

// -----------------  LPF TEST  ---------------------------

void *lpf_test(void *cx)
{
    int sample_rate, f, n;
    double *in, *in_fft;
    double *lpf, *lpf_fft;
    complex *in_complex, *in_fft_complex;
    complex *lpf_complex, *lpf_fft_complex;

    #define SECS 1

    sample_rate = 24000;
    n = SECS * sample_rate;

    // allocate buffers
    in      = fftw_alloc_real(n);
    in_fft  = fftw_alloc_real(n);
    lpf     = fftw_alloc_real(n);
    lpf_fft = fftw_alloc_real(n);

    in_complex = fftw_alloc_complex(n);
    in_fft_complex = fftw_alloc_complex(n);
    lpf_complex = fftw_alloc_complex(n);
    lpf_fft_complex = fftw_alloc_complex(n);

    // create sum of sine waves, to 'in' and 'in_complex' buffs
    zero_real(in, n);
    for (f = 990; f <= 12000; f += 1000) {
        add_sine_wave(sample_rate, f, n, in);
    }
    normalize(in, n, -1, 1);
    for (int i = 0; i < n; i++) {
        in_complex[i] = in[i];
    }

    // perfrom fwd fft on 'in' to 'in_fft', and plot
    fft_fwd_r2r(in, in_fft, n);
    normalize(in_fft, n, 0, 1);
    PLOT(0, false, in_fft, n,  0, n,  0, +1,  "IN_FFT");

    // perform lpf on 'in' to 'lpf', and plot the fft
    fft_lpf_real(in, lpf, n, sample_rate, 1500);
    fft_fwd_r2r(lpf, lpf_fft, n);
    normalize(lpf_fft, n, 0, 1);
    PLOT(1, false, lpf_fft, n,  0, n,  0, +1,  "LPF_FFT");

    // perfrom fwd fft on 'in_complex' to 'in_fft_complex', and plot
    fft_fwd_c2c(in_complex, in_fft_complex, n);
    PLOT(2, true, in_fft_complex, n,  0, n,  0, +1,  "IN_FFT_COMPLEX");

    // perform lpf on 'in_complex' to 'lpf_complex' and plot the fft
    fft_lpf_complex(in_complex, lpf_complex, n, sample_rate, 1500);
    fft_fwd_c2c(lpf_complex, lpf_fft_complex, n);
    PLOT(3, true, lpf_fft_complex, n,  0, n,  0, +1,  "LPF_FFT_COMPLEX");

    return NULL;
}

// -----------------  AUDIO TEST  ---------------------------

void *audio_test(void *cx)
{
    double *data;
    int num_items, sample_rate, n, idx;
    double *in, *out, *out_fft;

    // read wav file
    read_and_filter_wav_file("super_critical.wav", &data, &num_items, &sample_rate, 4000);

    // init lpf_freq control
    tc.int_name = "LPF_FREQ";
    tc.int_val  = 4000;
    tc.int_step = 100;
    tc.int_min  = 500;
    tc.int_max  = 10000;

    // xxx
    n = sample_rate / 10;
    float audio_out[n];
    idx = 0;

    // alloc buffers
    in = fftw_alloc_real(n);
    out = fftw_alloc_real(n);
    out_fft = fftw_alloc_real(n);

    // loop forever, writing wav file audio to stdout
    while (true) {
        memcpy(in, data+idx, n*sizeof(double));

        fft_lpf_real(in, out, n, sample_rate, tc.int_val);
        for (int i = 0; i < n; i++) {
            out[i] /= n;
        }

        fft_fwd_r2r(out, out_fft, n);
        normalize(out_fft, n, 0, 1);
        PLOT(0, false, out_fft, n,  0, sample_rate,  0, 1, "XXX");

        for (int i = 0; i < n; i++) {
            audio_out[i] = out[i];
        }
        fwrite(audio_out, sizeof(float), n, stdout);

        idx += n;
        if (idx + n >= num_items) {
            idx = 0;
        }
    }

    return NULL;
}

// -----------------  GEN TEST  -----------------------------

// xxx dont use all these defines
#define SAMPLE_RATE  2400000   // 2.4 MS/sec
#define F_SYNTH       600000   // 600 KHz
#define F_LPF        (SAMPLE_RATE / 4)
#define MAX_IQ       (SAMPLE_RATE / 10)   // 0.1 sec range
#define DELTA_T      (1. / SAMPLE_RATE)
#define W_SYNTH      (TWO_PI * F_SYNTH)

void *gen_test(void *cx)
{
    complex *iq, *iq_fft;
    double   t, max_iq_scaling, max_iq_measured=0;
    double   *antenna, synth0, synth90;
    int      i=0, tmax;
    FILE    *fp;

    static unsigned char usb[2*MAX_IQ];

    tmax = (arg1 == -1 ? 10 : arg1);
    max_iq_scaling = (arg2 <= 0 ? 750000 : arg2);

    init_antenna();

    iq     = fftw_alloc_complex(MAX_IQ);
    iq_fft = fftw_alloc_complex(MAX_IQ);
    antenna = fftw_alloc_real(MAX_IQ);

    fp = fopen("gen.dat", "w");
    if (fp == NULL) {
        FATAL("failed to create gen.dat\n");
    }

    NOTICE("generating %d secs of data, max_iq_scaling=%0.2f\n", tmax, max_iq_scaling);

    for (t = 0; t < tmax; t += DELTA_T) {
        antenna[i] = get_antenna(t);
        synth0     = sin((W_SYNTH) * t);
        synth90    = sin((W_SYNTH) * t + M_PI_2);
        iq[i]      = antenna[i] * (synth0 + I * synth90);
        i++;

        if (i == MAX_IQ) {
            sprintf(tc.title, "gen %0.1f secs", t);

            fft_lpf_complex(iq, iq, MAX_IQ, SAMPLE_RATE, F_LPF);

            // plots
            PLOT(0, false, antenna, MAX_IQ,  0, 999,  -2, 2,  "ANTENNA");  // x scale
            fft_fwd_c2c(iq, iq_fft, MAX_IQ);
            PLOT(1, true, iq_fft, MAX_IQ,  0, 999,  0, 1,  "IQ_FFT");

            int offset = MAX_IQ/12;
            int span  = (long)24000 * MAX_IQ / SAMPLE_RATE;
            PLOT(2, true, iq_fft+(offset-span/2), span,  0, 999,  0, 1,  "IQ_FFT");
            // xxx ctr plot3
            PLOT(3, true, iq_fft, span,  0, 999,  0, 1,  "IQ_FFT");

            double scaling = 128. / max_iq_scaling;
            for (int j = 0; j < MAX_IQ; j++) {
                if (cabs(iq[j]) > max_iq_measured) max_iq_measured = cabs(iq[j]);

                int inphase    = nearbyint(128 + scaling * creal(iq[j]));
                int quadrature = nearbyint(128 + scaling * cimag(iq[j]));
                if (inphase < 0 || inphase > 255 || quadrature < 0 || quadrature > 255) {
                    static int count=0;
                    if (count++ < 10) {
                        ERROR("iq val too large %d %d", inphase, quadrature);
                    }
                }

                usb[2*j+0] = inphase;
                usb[2*j+1] = quadrature;
                DEBUG("%u %u\n", usb[2*j+0], usb[2*j+1]);
            }

            fwrite(usb, sizeof(unsigned char), MAX_IQ*2, fp);

            i = 0;
        }
    }

    fclose(fp);

    NOTICE("max_iq_measured = %f\n", max_iq_measured);
    NOTICE("max_iq_scaling  = %f\n", max_iq_scaling);
    if (max_iq_measured > max_iq_scaling) {
        ERROR("max_iq_scaling is too small\n");
    }

    return NULL;
}

// -----------------  RX TEST  -----------------------------

// reference sim/sim.c

// xxx
// - start / stop ctrls

#define BLOCK_SIZE 262144
#define MAX_DATA (4*BLOCK_SIZE)

unsigned int sample_rate = 2400000;  // xxx make this private for each section

unsigned long head;
unsigned long tail;
unsigned char data[MAX_DATA];  // xxx rename to Data ?

void init_get_data(void);
void *get_data_from_file_thread(void *cx);
void audio_out(double yo);

void *rx_test(void *cx)
{
    struct {
        unsigned char i;
        unsigned char q;
    } *d;

    complex *dc;
    int n;
    double yo=0, y;
    int cnt=0;

    init_get_data();

    dc = fftw_alloc_complex(BLOCK_SIZE/2);

    while (true) {
        // wait for data
        while (head == tail) {
            usleep(1000);
        }

        d = (void*)&data[head%MAX_DATA];
        n = BLOCK_SIZE/2;
        //NOTICE("GOT DATA\n");

        for (int i = 0; i < n; i++) {
            dc[i] = ((d[i].i - 128) / 128.) + 
                    ((d[i].q - 128) / 128.) * I;
        }

        // process the usb data ...

        // low pass filter
        //fft_lpf_complex(dc, dc, n, sample_rate, 4000);

        for (int i = 0; i < n; i++) {
            //y = creal(dc[i]) * 5e-5;
            y = creal(dc[i]);

            if (y > yo) {
                yo = y;
            }
            yo = .9995 * yo;

            if (cnt++ == (sample_rate / 22000)) {
                audio_out(yo);
                cnt = 0;
            }
        }

        __sync_synchronize();
        head += BLOCK_SIZE;
        __sync_synchronize();
    }
}

void audio_out(double yo)
{
    #define MAX_MA 1000
    #define MAX_OUT 1000

    static void *ma_cx;
    static float out[MAX_OUT];
    static int max;

    double ma;

    ma = moving_avg(yo, MAX_MA, &ma_cx);
    out[max++] = yo - ma;

    if (max == MAX_OUT) {
        fwrite(out, sizeof(float), MAX_OUT, stdout);
#if 1
        double out_min, out_max, out_avg;
        average_float(out, MAX_OUT, &out_min, &out_max, &out_avg);
        fprintf(stderr, "min=%f max=%f avg=%f\n", out_min, out_max, out_avg);
#endif
        max = 0;
    }
}


void init_get_data(void)
{
    pthread_t tid;

    // will support getting data from:
    // - file
    // - tcp connection to rtl_sdr_server
    // - directly from rtl_sdr device

    // for now, get data from gen.dat
    pthread_create(&tid, NULL, get_data_from_file_thread, NULL);
}

void *get_data_from_file_thread(void *cx)
{
    int fd;
    unsigned long t_read, t_next_read, t_delay;;
    struct stat statbuf;
    size_t file_size, file_offset, len;

    //unsigned long t_start, total=0;

    fd = open("gen.dat", O_RDONLY);
    if (fd < 0) {
        FATAL("failed open gen.dat, %s\n", strerror(errno));
    }

    fstat(fd, &statbuf);
    file_size = statbuf.st_size;
    file_offset = 0;

    //t_start = microsec_timer();

    t_read = microsec_timer();
    while (true) {
        // if there is room in data buffer then read from file
        if ((MAX_DATA - (tail - head)) >= BLOCK_SIZE) {
            len = pread(fd, data+(tail%MAX_DATA), BLOCK_SIZE, file_offset);
            if (len != BLOCK_SIZE) {
                FATAL("read gen.dat, len=%ld, %s\n", len, strerror(errno));
            }

            //NOTICE("put data\n");

            __sync_synchronize();
            tail += BLOCK_SIZE;
            __sync_synchronize();

            file_offset += BLOCK_SIZE;
            if (file_offset + BLOCK_SIZE > file_size) {
                file_offset = 0;
            }
        } else {
            ERROR("discarding\n");
        }

        // wait for time to do next read
        t_next_read = t_read + ((double)(BLOCK_SIZE/2) / sample_rate) * 1000000;
        t_delay = t_next_read - microsec_timer();
        if (t_delay > 0) {
            usleep(t_delay);
        }
        t_read = t_next_read;

        // debug print the read rate
        //total += BLOCK_SIZE;
        //NOTICE("rate = %e\n", (double)total/(microsec_timer() - t_start));
    }
}

