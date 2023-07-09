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

// xxx improve logging
// xxx comments

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

//static bool sdr_read_is_active;

//
// prototypes
//

static void real_set_ctr_freq(freq_t f);
static void real_read_sync(complex *data, int items);
static void real_read_async(sdr_async_rb_t *rb);
static void real_cancel_async(void);

static void sim_set_ctr_freq(freq_t f);
static void sim_read_sync(complex *data, int items);
static void sim_read_async(sdr_async_rb_t *rb);
static void sim_cancel_async(void);

// ---------------------------------------------------------------------
// -----------------  INIT  --------------------------------------------
// ---------------------------------------------------------------------

static void exit_hndlr(void);
static void sdr_print_dev_info(void);

void sdr_init(int dev_idx, int sample_rate)
{
    int rc;

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

    // xxx re-read these, and possibly the xtal_freq (above)
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

// ---------------------------------------------------------------------
// -----------------  LIST DEVICES  ------------------------------------
// ---------------------------------------------------------------------

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

// -----------------------------------------------------------------------
// -----------------  SDR HARDWARE TEST  ---------------------------------
// -----------------------------------------------------------------------

void sdr_hardware_test(void)
{
    unsigned char *buff, val;
    int rc, n_read, buff_len, err_count=0;
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
        ERROR("rtlsdr_reset_buffer failed, rc=%d\n", rc);  //xxx use a new macro for errors
        return;
    }

    // note: buff_len must be multiple of 512
    buff_len = (secs * (2 * info.sample_rate)) & ~0x200;
    buff = malloc(buff_len);
    memset(buff, 0, buff_len);
    
    NOTICE("reading test data ...\n");
    start = microsec_timer();
    rc = rtlsdr_read_sync(dev, buff, buff_len, &n_read);
    if (rc != 0) {
        ERROR("rtlsdr_read_sync failed, rc=%d\n", rc);
        return;
    }
    duration = microsec_timer() - start;
    NOTICE("done reading test data:  duration = %f s, n_read = %d, rate = %f MB/s\n",
           duration / 1000000., n_read, (double)n_read / duration);

    if (n_read != buff_len) {
        ERROR("n_read=0x%08x expected=0x%08x\n", n_read, buff_len);
        return;
    }

    NOTICE("checking test data\n");
    val = buff[2] + 1;
    for (int i = 3; i < buff_len; i++) {
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

// ---------------------------------------------------------------------
// -----------------  SDR CONTROL  -------------------------------------
// ---------------------------------------------------------------------

void sdr_set_ctr_freq(freq_t f, bool sim)
{
    if (sim) {
        sim_set_ctr_freq(f);
    } else {
        real_set_ctr_freq(f);
    }
}

void sdr_read_sync(complex *data, int n, bool sim)
{
    if (sim) {
        sim_read_sync(data, n);
    } else {
        real_read_sync(data, n);
    }
}

void sdr_read_async(sdr_async_rb_t *rb, bool sim)
{
    if (sim) {
        sim_read_async(rb);
    } else {
        real_read_async(rb);
    }
}

void sdr_cancel_async(bool sim)
{
    if (sim) {
        sim_cancel_async();
    } else {
        real_cancel_async();
    }
}

// ---------------------------------------------------------------------
// -----------------  REAL ROUTINES  -----------------------------------
// ---------------------------------------------------------------------

// ---- set ctr freq ----

static void real_set_ctr_freq(freq_t f)
{
    rtlsdr_set_center_freq(dev, f);
}

// ---- read sync ----

static void real_read_sync(complex *data, int items)
{
    static unsigned char *buff;
    static int            buff_len_alloced;

    int bytes_read, buff_len, i;
    
    buff_len = round_up(items * 2, 512);

    if (buff == NULL || buff_len > buff_len_alloced) {
        NOTICE("ALLOCING %d\n", buff_len);
        free(buff);
        buff = malloc(buff_len);
        buff_len_alloced = buff_len;
    }

    rtlsdr_reset_buffer(dev);

    bytes_read = 0;
    rtlsdr_read_sync(dev, buff, buff_len, &bytes_read);

    if (bytes_read != buff_len) {
        WARN("sdr_read_sync buff_len=0x%x bytes_read=0x%x\n", buff_len, bytes_read);
    }

    for (i = 0; i < items; i++) {
        data[i] = ((buff[2*i] - 128.) + (buff[2*i+1] - 128.) * I) / 128.;
    }
}

// ---- read async ----

// vars
static pthread_t real_async_read_thread_tid;

// prototypes
static void *real_async_read_thread(void *cx);
static void real_async_read_cb(unsigned char *buff, unsigned int len, void *cx);

static void real_read_async(sdr_async_rb_t *rb)
{
    rb->head = rb->tail = 0;
    pthread_create(&real_async_read_thread_tid, NULL, real_async_read_thread, rb);
}

static void real_cancel_async(void)
{
    rtlsdr_cancel_async(dev);
    pthread_join(real_async_read_thread_tid, NULL);
}

static void *real_async_read_thread(void *cx)
{
    rtlsdr_reset_buffer(dev);
    rtlsdr_read_async(dev, real_async_read_cb, cx, 0, 0);
    return NULL;
}

static void real_async_read_cb(unsigned char *buff, unsigned int len, void *cx)
{
    // xxx clean up here
    static unsigned long start;
    static unsigned long total;
    static int cnt;

    sdr_async_rb_t *rb = cx;

    if (start == 0) {
        start = microsec_timer();
    }
    total += len/2;
    if (++cnt == 50) {
        NOTICE("RATE = %f\n", total / ((microsec_timer()-start)/1000000.) / 1000000.);
        cnt = 0;
    }

    unsigned long   rb_avail, n, rb_tail, items;

    items = len/2;

    // copy the data to the tail of ring buffer, 
    // for as much will fit
    rb_avail = MAX_SDR_ASYNC_RB - (rb->tail - rb->head);
    n = (rb_avail < items ? rb_avail : items);
    if (n != items) {
        WARN("discarding %ld samples\n", items-n);
    }
    rb_tail = rb->tail;
    for (int i = 0; i < n; i++) {
        rb->data[rb_tail % MAX_SDR_ASYNC_RB] = 
                ((buff[2*i] - 128.) + (buff[2*i+1] - 128.) * I) / 128.;
        rb_tail++;
    }
    rb->tail = rb_tail;
}

// ---------------------------------------------------------------------
// -----------------  SIMULATION ROUTINES  -----------------------------
// ---------------------------------------------------------------------

// vars
static freq_t    sim_ctr_freq;
pthread_t        sim_async_tid;
bool             sim_async_cancel;
sdr_async_rb_t * sim_async_rb;

// prototypes
static void *sim_async_read_thread(void *cx);
static void sim_get_antenna_data(complex *data, int n);

// - - - - sim set ctr freq - - - - 

static void sim_set_ctr_freq(freq_t f)
{
    sim_ctr_freq = f;
}

// - - - - sim sync read - - - - 

static void sim_read_sync(complex *data, int n)
{
    sim_get_antenna_data(data, n);
    usleep(1000000 * (n * DELTA_T));
}

// - - - - sim async read - - - - 

static void sim_read_async(sdr_async_rb_t *rb)
{
    if (sim_async_tid != 0) {
        FATAL("sdr sim async read is already active\n");
    }

    sim_async_cancel = false;
    sim_async_rb = rb;

    // xxx detached
    pthread_create(&sim_async_tid, NULL, sim_async_read_thread, NULL);
}

static void sim_cancel_async(void)
{
    if (sim_async_tid == 0) {
        FATAL("sdr sim async read is not active\n");
    }

    sim_async_cancel = true;

    pthread_join(sim_async_tid, NULL);

    sim_async_cancel = false;
    sim_async_rb = NULL;
    sim_async_tid = 0;
}

static void *sim_async_read_thread(void *cx)
{
    #define MAX_DATA 131072

    sdr_async_rb_t *rb = sim_async_rb;
    complex         data[MAX_DATA];
    unsigned long   rb_avail, n, rb_tail;
    unsigned long   delta_t, target, now;

    pthread_setname_np(pthread_self(), "sdr_async_read");

    NOTICE("async read thread starting\n");

    delta_t = nearbyint((double)MAX_DATA / info.sample_rate * 1000000);
    target = microsec_timer() + delta_t;
    DEBUG("delta_t = %ld\n", delta_t);

    while (true) {
        // check for cancel request
        if (sim_async_cancel) {
            goto terminate;
        }

        // get a block of simulated antenna data
        sim_get_antenna_data(data, MAX_DATA);

        // wait for it to be time to provide the data
        now = microsec_timer();
        if (target > now) {
            usleep(target - now);
            DEBUG("slept for %ld us\n", target-now);
        } else {
            WARN("target is before now\n");
        }
        target += delta_t;

        // copy the data to the tail of ring buffer, 
        // for as much will fit
        rb_avail = MAX_SDR_ASYNC_RB - (rb->tail - rb->head);
        n = (rb_avail < MAX_DATA ? rb_avail : MAX_DATA);
        if (n != MAX_DATA) {
            WARN("discarding %ld samples\n", MAX_DATA-n);
        }
        rb_tail = rb->tail;
        for (int i = 0; i < n; i++) {
            rb->data[rb_tail % MAX_SDR_ASYNC_RB] = data[i];  //xxx more efficient?
            rb_tail++;
        }
        rb->tail = rb_tail;
    }

terminate:
    NOTICE("async read thread terminating\n");

    return NULL;
}

// - - - - sim get antenna data - - - - 

static double      *antenna;
static unsigned int max_antenna;

static void sim_get_antenna_data_init(void);

static void sim_get_antenna_data(complex *data, int n)
{
    static unsigned int idx;
    static double t;
    double w = TWO_PI * sim_ctr_freq;

    // init the antenna data on first call
    sim_get_antenna_data_init();

    // shift the frequency to sim_ctr_freq, and copy to caller's buffer
    for (int i = 0; i < n; i++) {
        data[i] = antenna[idx++] * cexp(-I * w * t);
        if (idx == max_antenna) idx = 0;
        t += DELTA_T;
    }
}

static void sim_get_antenna_data_init(void)
{
    int fd;
    size_t len_read;
    size_t file_size;
    struct stat statbuf;

    // if already initialized then return
    if (antenna) {
        return;
    }

    // read the entire sim.dat file
    fd = open("sim/sim.dat", O_RDONLY);
    if (fd < 0) {
        FATAL("failed open sim/sim.dat, %m\n");
    }

    fstat(fd, &statbuf);
    file_size = statbuf.st_size;

    antenna = malloc(file_size);
    max_antenna = file_size / sizeof(double);

    len_read = read(fd, antenna, file_size);
    if (len_read != file_size) {
        FATAL("read antenna file, len=%zd, %m\n", len_read);
    }

    close(fd);
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

#endif
