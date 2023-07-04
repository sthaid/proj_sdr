#include "common.h"

#include <rtl-sdr.h>

// xxx add references
// https://pa3fwm.nl/technotes/tn20.html

// xxx todo
// - add link to descriptions
// - check ret codes
// - use these atexit
//     rc = rtlsdr_cancel_async(dev);
//     rtlsdr_close(dev);

// xxx maybe also use
// - rtlsdr_set_tuner_if_gain
// - rtlsdr_get_freq_correction
// - rtlsdr_set_offset_tuning
// - rtlsdr_get_offset_tuning
// - rtlsdr_set_bias_tee

//
// defines
//

#define F_DS 28800000   // 28.8 MHz

#define TUNER_TYPE_STR(x) \
    ((x) == RTLSDR_TUNER_UNKNOWN ? "UNKNOWN" : \
     (x) == RTLSDR_TUNER_E4000   ? "E4000"   : \
     (x) == RTLSDR_TUNER_FC0012  ? "FC0012"  : \
     (x) == RTLSDR_TUNER_FC0013  ? "FC0013"  : \
     (x) == RTLSDR_TUNER_FC2580  ? "FC2500"  : \
     (x) == RTLSDR_TUNER_R820T   ? "R820T"   : \
     (x) == RTLSDR_TUNER_R828D   ? "R828D"   : \
                                    "????")

#define TUNER_GAIN_MODE_AUTO   0
#define TUNER_GAIN_MODE_MANUAL 1

#define RTL2832_AGC_MODE_ENABLE  1
#define RTL2832_AGC_MODE_DISABLE 0

#define DIRECT_SAMPLING_DISABLED          0
#define DIRECT_SAMPLING_I_ADC_ENABLED     1
#define DIRECT_SAMPLING_Q_ADC_ENABLED     2

#define DIRECT_SAMPLING_STR(x) \
    ((x) == DIRECT_SAMPLING_DISABLED      ? "DISABLED"      : \
     (x) == DIRECT_SAMPLING_I_ADC_ENABLED ? "I_ADC_ENABLED" : \
     (x) == DIRECT_SAMPLING_Q_ADC_ENABLED ? "Q_ADC_ENABLED" : \
                                            "????")

#define TEST_MODE_ON  1
#define TEST_MODE_OFF 0

#define DELTA_T (1. / info.sample_rate)

//
// typedef
//

//
// variables
//

static rtlsdr_dev_t *dev;

static struct {
    // static fields
    int  dev_idx;
    char dev_name[256];
    char manufact[256];
    char product[256];
    char serial[256];
    int  tuner_type;
    int  tuner_gains[100];  // units = tenth db
    int  num_tuner_gains;
    unsigned int rtl2832_xtal_freq;
    unsigned int tuner_xtal_freq;

    // configurable fields
    int  sample_rate;
    bool rtl2832_agc_enabled;
    bool tuner_gain_mode_manual;
    int  tuner_gain;        // units = tenth db
    int  direct_sampling;
    int  ctr_freq;
} info;

static bool sdr_read_is_active;

//
// prototypes
//

static void exit_hndlr(void);
static void sdr_print_dev_info(void);

static void sim_sdr_set_ctr_freq(freq_t f);
static void sim_sdr_read_sync(complex *data, int n);
static void sim_sdr_read_async(sdr_async_rb_t *rb);
static void sim_sdr_cancel_async(void);

// -----------------  INIT AND LIST_DEVICES  ---------------------------

void sdr_init(int dev_idx, int sample_rate)
{
    int rc;

    if (dev != NULL) {
        FATAL("sdr_init has already been called\n");
    }

    // open
    rc = rtlsdr_open(&dev, dev_idx);
    if (rc != 0) {
        FATAL("rtlsdr_open rc=%d\n", rc);
    }

    // get info
    // - dev idx and name
    info.dev_idx = dev_idx;
    strcpy(info.dev_name, rtlsdr_get_device_name(dev_idx));
    // - manufact, product, serial
    rtlsdr_get_usb_strings(dev, info.manufact, info.product, info.serial);
    // - tuner type
    info.tuner_type = rtlsdr_get_tuner_type(dev);
    // - tuner gains
    info.num_tuner_gains = rtlsdr_get_tuner_gains(dev, info.tuner_gains);
    // - xtal freq
    rtlsdr_get_xtal_freq(dev, &info.rtl2832_xtal_freq, &info.tuner_xtal_freq);

    // disable agc mode of the RTL2832
    rtlsdr_set_agc_mode(dev, RTL2832_AGC_MODE_DISABLE);
    info.rtl2832_agc_enabled = false;

    // set tuner manual gain mode, and set tuner gain to max
    rtlsdr_set_tuner_gain_mode(dev, TUNER_GAIN_MODE_MANUAL);
    rtlsdr_set_tuner_gain(dev, info.tuner_gains[info.num_tuner_gains-1]);
    info.tuner_gain_mode_manual = true;

    // set sample rate
    rtlsdr_set_sample_rate(dev, sample_rate);

    // disable direct sampling mode
    rtlsdr_set_direct_sampling(dev, DIRECT_SAMPLING_DISABLED);

    // xxx ctr_freq not set

    // readback the dynamic values that can be read back
    info.sample_rate = rtlsdr_get_sample_rate(dev);
    info.tuner_gain = rtlsdr_get_tuner_gain(dev);
    info.direct_sampling = rtlsdr_get_direct_sampling(dev);
    info.ctr_freq = rtlsdr_get_center_freq(dev);

    // print info
    sdr_print_dev_info();

    // register exit_hndlr
    atexit(exit_hndlr);
}

static void exit_hndlr(void)
{
    NOTICE("%s exit_hndlr\n", __FILE__);

    if (dev) {
        rtlsdr_close(dev);
    }
}

static void sdr_print_dev_info(void)
{
    char tuner_gains_str[1000], *p;

    // if sdr_init was not called then error
    if (dev == NULL) {
        FATAL("no dev\n");
    }

    // construct string of supported tuner gain values
    p = tuner_gains_str;
    for (int i = 0; i < info.num_tuner_gains; i++) {
        p += sprintf(p, "%d ", info.tuner_gains[i]);
    }

    // xxx if gain mode is auto may need to read the gain again

    NOTICE("---------- Static Values ----------\n");
    NOTICE("dev_idx         = %d\n",
           info.dev_idx);
    NOTICE("dev_name        = %s\n",
           info.dev_name);
    NOTICE("dev strings     = manfact='%s'  product='%s'  serial='%s'\n",
           info.manufact,
           info.product,
           info.serial);
    NOTICE("tuner_type      = %s\n", 
           TUNER_TYPE_STR(info.tuner_type));
    NOTICE("tuner_gains     = %s\n", 
           tuner_gains_str);
    NOTICE("xtal_freq       = %d (rtl2832)  %d (tuner)\n",
           info.rtl2832_xtal_freq, 
           info.tuner_xtal_freq);

    NOTICE("---------- Configurable Values ----------\n");
    NOTICE("sample_rate     = %d\n",
           info.sample_rate);
    NOTICE("rtl2832_agc     = %s\n",
           info.rtl2832_agc_enabled ? "enabled" : "disabled");
    NOTICE("tuner_gain      = %d (%s)\n",
           info.tuner_gain, 
           info.tuner_gain_mode_manual ? "manual" : "auto");
    NOTICE("direct_sampling = %s\n",
           DIRECT_SAMPLING_STR(info.direct_sampling));
    NOTICE("ctr_freq        = %d\n",
           info.ctr_freq);
}

void sdr_list_devices(void)
{
    int dev_cnt, idx;

    dev_cnt = rtlsdr_get_device_count();
    if (dev_cnt == 0) {
        ERROR("no sdr devices found\n");
    }

    for (idx = 0; idx < dev_cnt; idx++) {
        char manufact[256]={0}, product[256]={0}, serial[256]={0};
        rtlsdr_get_device_usb_strings(idx, manufact, product, serial);
        NOTICE("dev_idx=%d dev_name='%s' manufact='%s' product='%s' serial='%s'\n",
               idx, rtlsdr_get_device_name(idx), manufact, product, serial);
    }
}

// -----------------  SDR HARDWARE TEST  ---------------------------------

void sdr_hardware_test(void)
{
    unsigned char *buff, val;
    int rc, n_read, buff_len_desired, buff_len_padded, err_count=0;
    const int secs = 1;
    unsigned long start, duration;

    NOTICE("---------- SDR HARDWARE TEST ----------\n");

    NOTICE("enabling test mode\n");
    rc = rtlsdr_set_testmode(dev, TEST_MODE_ON);
    if (rc != 0) {
        ERROR("failed to enable test mode\n");
        return;
    }

    rc = rtlsdr_reset_buffer(dev);
    if (rc != 0) {
        ERROR("failed to reset buffer\n");
        return;
    }

    buff_len_desired = secs * (2 * info.sample_rate);
    buff_len_padded  = buff_len_desired + 50000;
    buff = malloc(buff_len_padded);
    memset(buff, 0, buff_len_padded);
    
    NOTICE("reading test data ...\n");
    start = microsec_timer();
    rtlsdr_read_sync(dev, buff, buff_len_padded, &n_read);
    duration = microsec_timer() - start;
    NOTICE("done reading test data:  duration = %f s, n_read = %d, rate = %f MB/s\n",
           duration / 1000000., n_read, (double)n_read / duration);

    if (n_read < buff_len_desired) {
        ERROR("n_read=%d is too small, expected=%d\n", n_read, buff_len_desired);
        return;
    }

    NOTICE("checking test data\n");
    val = buff[0];
    for (int i = 0; i < buff_len_desired; i++) {
        if (buff[i] != val) {
            if (++err_count < 10) {
                ERROR("buff[%d...%d] = %d %d %d %d %d %d %d\n", 
                    i-3, i+3, buff[i-3], buff[i-2], buff[i-1], buff[i-0], buff[i+1], buff[i+2], buff[i+3]);
            }
            val = buff[i];
        }
        val++;
    }
    NOTICE("number of errors detected = %d : %s\n", 
           err_count,
           err_count <= 3 ? "TEST PASSED" : "TEST FAILED");

    free(buff);
}

// -----------------  SDR SET CENTER FREQ  -----------------------------

void sdr_set_ctr_freq(freq_t f, bool sim)
{
    if (sim) {
        sim_sdr_set_ctr_freq(f);
        return;
    }

    FATAL("not coded");
}

// -----------------  SDR SYNC READ  -----------------------------------

void sdr_read_sync(complex *data, int n, bool sim)
{
    if (sdr_read_is_active) {
        FATAL("sdr read is active\n");
        return;
    }

    if (sim) {
        sdr_read_is_active = true;
        sim_sdr_read_sync(data, n);
        sdr_read_is_active = false;
        return;
    }

    FATAL("not coded\n");
    // xxx, be sure to get the full buffer
    //rtlsdr_read_sync(dev, buff, buff_len, &n_read);
}

// -----------------  SDR ASYNC READ  ----------------------------------

void sdr_read_async(sdr_async_rb_t *rb, bool sim)
{
    if (sdr_read_is_active) {
        FATAL("sdr read is active\n");
        return;
    }

    memset(rb, 0, sizeof(sdr_async_rb_t));

    if (sim) {
        sim_sdr_read_async(rb);
        sdr_read_is_active = true;
        return;
    }

    FATAL("not coded\n");
}

void sdr_cancel_async(bool sim)
{
    if (!sdr_read_is_active) {
        FATAL("sdr read is not active\n");
        return;
    }

    if (sim) {
        sim_sdr_cancel_async();
        sdr_read_is_active = false;
        return;
    }

    FATAL("not coded\n");
}

// ---------------------------------------------------------------------
// -----------------  SIMULATION  --------------------------------------
// ---------------------------------------------------------------------

static struct {
    pthread_t       tid;
    bool            cancel;
    sdr_async_rb_t *rb;
} sim_async;

static freq_t sim_ctr_freq;

static void *sim_async_read_thread(void *cx);
static void sim_get_antenna_data(complex *data, int n);

// - - - - sim set ctr freq - - - - 

static void sim_sdr_set_ctr_freq(freq_t f)
{
    sim_ctr_freq = f;
}

// - - - - sim sync read - - - - 

static void sim_sdr_read_sync(complex *data, int n)
{
    sim_get_antenna_data(data, n);
    usleep(1000000 * (n * DELTA_T));
}

// - - - - sim async read - - - - 

static void sim_sdr_read_async(sdr_async_rb_t *rb)
{
    if (sim_async.tid != 0) {
        ERROR("sdr sim async read is already active\n");
        return;
    }

    sim_async.cancel = false;
    sim_async.rb = rb;

    pthread_create(&sim_async.tid, NULL, sim_async_read_thread, NULL);
}

static void sim_sdr_cancel_async(void)
{
    // xxx make sure the tid is set
    sim_async.cancel = true;

    pthread_join(sim_async.tid, NULL);

    sim_async.cancel = false;
    sim_async.rb = NULL;
    sim_async.tid = 0;
}

static void *sim_async_read_thread(void *cx)
{
    #define MAX_DATA 131072

    sdr_async_rb_t *rb = sim_async.rb;
    complex         data[MAX_DATA];

    pthread_setname_np(pthread_self(), "sdr_async_read");

    NOTICE("async read thread starting\n");

    // xxx feed the simulated data at the sdr sample rate

    while (true) {
        // get a block of simulated antenna data
        sim_get_antenna_data(data, MAX_DATA);

        // wait for room in the ring buffer
        // xxx maybe just wait for any room
        while (true) {
            if (sim_async.cancel) {
                goto terminate;
            }

            int rb_avail = MAX_SDR_ASYNC_RB - (rb->tail - rb->head);
            if (rb_avail >= MAX_DATA) {
                break;
            }

            usleep(1000);
        }

        // copy the data to the tail of ring buffer
        unsigned long rb_tail = rb->tail;
        for (int i = 0; i < MAX_DATA; i++) {
            rb->data[rb_tail % MAX_SDR_ASYNC_RB] = data[i];
            rb_tail++;
        }
        rb->tail = rb_tail;

        // check for cancel request
        if (sim_async.cancel) {
            goto terminate;
        }
    }

terminate:
    NOTICE("async read thread terminating\n");

    return NULL;
}

// - - - - sim get antenna data - - - - 

static void sim_get_antenna_data(complex *data, int n)
{
    static bool first_call = true;
    static double *antenna;
    static unsigned int idx, max;
    static double t;

    // read the entire sim.dat file on first_call
    if (first_call) {
        int fd;
        size_t len_read;
        size_t file_size;
        struct stat statbuf;

        fd = open("sim/sim.dat", O_RDONLY);
        if (fd < 0) {
            FATAL("failed open sim/sim.dat, %m\n");
        }

        fstat(fd, &statbuf);
        file_size = statbuf.st_size;

        antenna = malloc(file_size);

        len_read = read(fd, antenna, file_size);
        if (len_read != file_size) {
            FATAL("read antenna file, len=%zd, %m\n", len_read);
        }

        close(fd);

        first_call = false;
        idx        = 0;
        max        = file_size / sizeof(double);
        t          = 0;
    }

    // shift the frequency to sim_ctr_freq, and copy to caller's buffer
    double w = TWO_PI * sim_ctr_freq;
    for (int i = 0; i < n; i++) {
        data[i] = antenna[idx++] * cexp(-I * w * t);
        if (idx == max) idx = 0;
        t += DELTA_T;
    }
}

// --------------------------------------
// --------------------------------------
// --------------------------------------
// --------------------------------------
// --------------------------------------
#if 0 //xxx
static void(*cb)(unsigned char *iq, size_t len);
static bool direct_sampling_enabled;

static void print_info(rtlsdr_dev_t *dev);
static void get_gains(rtlsdr_dev_t *dev, int *num_gains_arg, int **gains_arg);
static void *async_reader_thread(void *cx);
static void async_reader_cb(unsigned char *buf, unsigned int len, void *cx_arg);
#endif

#if 0
    // enable direct sampling
    int direct_sampling;
    direct_sampling = (f < F_DS ? DIRECT_SAMPLING_Q_ADC_ENABLED : DIRECT_SAMPLING_DISABLED);
    NOTICE("curr direct sampling = %s\n", DIRECT_SAMPLING_STR(direct_sampling));
    direct_sampling_enabled = (direct_sampling == DIRECT_SAMPLING_Q_ADC_ENABLED);

xxx other routine needed for the remaining
    // set center frequency
    unsigned int ctr_freq;
    rtlsdr_set_center_freq(dev, f);
    ctr_freq = rtlsdr_get_center_freq(dev);
    NOTICE("curr ctr_freq %d  %f MHZ\n", ctr_freq, (double)ctr_freq/MHZ);

    // start async reader
    pthread_t tid;
    rc = rtlsdr_reset_buffer(dev);
    if (rc != 0) {
        FATAL("rtlsdr_reset_buffer\n");
    }
    pthread_create(&tid, NULL, async_reader_thread, NULL);
}

void sdr_set_freq(double f)
{
    bool      direct_sampling_needed = (f < F_DS);
    int       rc, direct_sampling;
    pthread_t tid;

    if (direct_sampling_enabled != direct_sampling_needed) {
        NOTICE("CANCELLING ASYNC\n");
        rc = rtlsdr_cancel_async(dev);
        if (rc != 0) {
            FATAL("rtlsdr_cancel_async\n");
        }
        sleep(1);

        direct_sampling = (f < F_DS ? DIRECT_SAMPLING_Q_ADC_ENABLED : DIRECT_SAMPLING_DISABLED);
        NOTICE("SETTING DIRECT SAMPLING TO %s\n", DIRECT_SAMPLING_STR(direct_sampling));
        rc = rtlsdr_set_direct_sampling(dev, direct_sampling);
        if (rc != 0) {
            FATAL("rtlsdr_set_direct_sampling\n");
        }
        direct_sampling_enabled = (direct_sampling == DIRECT_SAMPLING_Q_ADC_ENABLED);

        // start async reader
        NOTICE("STARTING ASYNC READER\n");
        rc = rtlsdr_reset_buffer(dev);
        if (rc != 0) {
            FATAL("rtlsdr_reset_buffer\n");
        }
        pthread_create(&tid, NULL, async_reader_thread, NULL);
    }

    rtlsdr_set_center_freq(dev, f);
}

// -----------------  LOCAL  -------------------------------------------

static void *async_reader_thread(void *cx)
{
    NOTICE("BEFORE rtlsdr_read_async\n");
    rtlsdr_read_async(dev, async_reader_cb, NULL, 0, 0);
    NOTICE("AFTER rtlsdr_read_async\n");

    return NULL;
}

static void async_reader_cb(unsigned char *buf, unsigned int len, void *cx)
{
    static unsigned long start;
    static unsigned long total;
    static int cnt;

    if (start == 0) {
        start = microsec_timer();
    }
    total += len/2;
    if (++cnt == 50) {
        NOTICE("RATE = %f\n", total / ((microsec_timer()-start)/1000000.) / 1000000.);
        cnt = 0;
    }

    cb(buf, len);
}

#endif
