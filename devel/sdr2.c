#include "common.h"
#include <rtl-sdr.h>

// xxx check ret codes
// xxx may want this when shutting down;  could use atexit
//  rc = rtlsdr_cancel_async(dev);
//  rtlsdr_close(dev);

//
// defines
//

#define SAMPLE_RATE   2400000

#define TUNER_TYPE_STR(x) \
    ((x) == RTLSDR_TUNER_UNKNOWN ? "UNKNOWN" : \
     (x) == RTLSDR_TUNER_E4000   ? "E4000"   : \
     (x) == RTLSDR_TUNER_FC0012  ? "FC0012"  : \
     (x) == RTLSDR_TUNER_FC0013  ? "FC0013"  : \
     (x) == RTLSDR_TUNER_FC2580  ? "FC2500"  : \
     (x) == RTLSDR_TUNER_R820T   ? "R820T"   : \
     (x) == RTLSDR_TUNER_R828D   ? "R828D"   : \
                                    "????")

#define AGC_MODE_ENABLE  1
#define AGC_MODE_DISABLE 0

#define DIRECT_SAMPLING_DISABLED          0
#define DIRECT_SAMPLING_I_ADC_ENABLED     1
#define DIRECT_SAMPLING_Q_ADC_ENABLED     2

#define DIRECT_SAMPLING_STR(x) \
    ((x) == DIRECT_SAMPLING_DISABLED      ? "DISABLED"      : \
     (x) == DIRECT_SAMPLING_I_ADC_ENABLED ? "I_ADC_ENABLED" : \
     (x) == DIRECT_SAMPLING_Q_ADC_ENABLED ? "Q_ADC_ENABLED" : \
                                            "????")

#define KHZ 1000
#define MHZ 1000000

//
// typedef
//

//
// variables
//

static rtlsdr_dev_t *dev;
static void(*cb)(unsigned char *iq, size_t len);
static bool direct_sampling_enabled;

//
// prototypes
//

static void print_info(rtlsdr_dev_t *dev);
static void get_gains(rtlsdr_dev_t *dev, int *num_gains_arg, int **gains_arg);
static int get_max_gain(rtlsdr_dev_t *dev);
static void *async_reader_thread(void *cx);
static void async_reader_cb(unsigned char *buf, unsigned int len, void *cx_arg);

// -----------------  API  ---------------------------------------------

// xxx call this at startup
void sdr_list_devices(void)
{
    int dev_cnt, i, rc;

    dev_cnt = rtlsdr_get_device_count();
    NOTICE("dev_cnt = %d\n", dev_cnt);
    if (dev_cnt == 0) {
        FATAL("dev_cnt = 0\n");
    }

    for (i = 0; i < dev_cnt; i++) {
        char manufact[256], product[256], serial[256];

        rc = rtlsdr_get_device_usb_strings(0, manufact, product, serial);
        if (rc != 0) {
            FATAL("rtlsdr_get_device_usb_strings(%d) rc=%d\n", i, rc);
        }
        NOTICE("name='%s'  manufact='%s'  product='%s'  serial='%s'\n",
               rtlsdr_get_device_name(i), manufact, product, serial);
    }
}

// xxx pass in idx, sample_rate, ...
void sdr_init(double f, void(*cb_arg)(unsigned char *iq, size_t len))
{
    int rc;

    // save cb_arg in global var
    cb = cb_arg;

    // open
    rc = rtlsdr_open(&dev, 0);
    if (rc != 0) {
        FATAL("rtlsdr_open rc=%d\n", rc);
    }
    NOTICE("opened index=0\n");

    // print info
    print_info(dev);

    // set gain mode manual, and set max_gain
    #define GAIN_MODE_AUTO   0
    #define GAIN_MODE_MANUAL 1
    int max_gain, curr_gain;
    // - set manual gain mode
    rtlsdr_set_tuner_gain_mode(dev, GAIN_MODE_MANUAL);
    // - set gain to max
    max_gain = get_max_gain(dev);
    rtlsdr_set_tuner_gain(dev, max_gain);
    // - readback and print gain to confirm
    curr_gain = rtlsdr_get_tuner_gain(dev);
    NOTICE("curr_gain = %d  max_gain = %d\n", curr_gain, max_gain);

    // set sample rate
    unsigned int curr_sample_rate;
    rtlsdr_set_sample_rate(dev, SAMPLE_RATE);
    curr_sample_rate = rtlsdr_get_sample_rate(dev);
    NOTICE("curr_sample_rate = %u\n", curr_sample_rate);

    // disable agc mode xxx should this be earlier
    rtlsdr_set_agc_mode(dev, AGC_MODE_DISABLE);

    // enable direct sampling
    int direct_sampling;
    direct_sampling = (f < 28.8*MHZ ? DIRECT_SAMPLING_Q_ADC_ENABLED : DIRECT_SAMPLING_DISABLED);
    rc = rtlsdr_set_direct_sampling(dev, direct_sampling);
    if (rc != 0) {
        FATAL("rtlsdr_set_direct_sampling\n");
    }
    direct_sampling = rtlsdr_get_direct_sampling(dev);
    NOTICE("curr direct sampling = %s\n", DIRECT_SAMPLING_STR(direct_sampling));
    direct_sampling_enabled = (direct_sampling == DIRECT_SAMPLING_Q_ADC_ENABLED);

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
    bool      direct_sampling_needed = (f < 28.8*MHZ);
    int       rc, direct_sampling;
    pthread_t tid;

    if (direct_sampling_enabled != direct_sampling_needed) {
        NOTICE("CANCELLING ASYNC\n");
        rc = rtlsdr_cancel_async(dev);
        if (rc != 0) {
            FATAL("rtlsdr_cancel_async\n");
        }
        sleep(1);

        direct_sampling = (f < 28.8*MHZ ? DIRECT_SAMPLING_Q_ADC_ENABLED : DIRECT_SAMPLING_DISABLED);
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

static void print_info(rtlsdr_dev_t *dev)
{
    int rc;
    char str[300], *p;

    // get xtal freq
    unsigned int rtl_freq, tuner_freq;
    rc = rtlsdr_get_xtal_freq(dev, &rtl_freq, &tuner_freq);
    if (rc != 0) {
        FATAL("rtlsdr_get_xtal_freq\n");
    }
    NOTICE("rtl_freq=%0.2f  tuner_freq=%0.2f\n", (double)rtl_freq/MHZ, (double)tuner_freq/MHZ);

    // get manufacturer
    char manufact[256], product[256], serial[256];
    rc = rtlsdr_get_usb_strings(dev, manufact, product, serial);
    NOTICE("manufact='%s'  product='%s'  serial='%s'\n",
           manufact, product, serial);

    // get tuner type
    enum rtlsdr_tuner tuner;
    tuner = rtlsdr_get_tuner_type(dev);
    NOTICE("tuner=%d %s\n", tuner, TUNER_TYPE_STR(tuner));   // is RTLSDR_TUNER_R820T

    // get gains
    int num_gains, *gains;
    get_gains(dev, &num_gains, &gains);
    p = str;
    p += sprintf(p, "num_gains=%d: ", num_gains);
    for (int i = 0; i < num_gains; i++) {
        p += sprintf(p, "%d ", gains[i]);
    }
    NOTICE("%s\n", str);
    NOTICE("max_gain = %d\n", gains[num_gains-1]);
    free(gains);
}

static void get_gains(rtlsdr_dev_t *dev, int *num_gains_arg, int **gains_arg)
{
    int num_gains, *gains;

    num_gains = rtlsdr_get_tuner_gains(dev, NULL);
    gains = calloc(num_gains, sizeof(int));
    rtlsdr_get_tuner_gains(dev, gains);

    *num_gains_arg = num_gains;
    *gains_arg = gains;
}

static int get_max_gain(rtlsdr_dev_t *dev)
{
    int num_gains, *gains, max_gain;

    get_gains(dev, &num_gains, &gains);
    max_gain = gains[num_gains-1];
    free(gains);
    return max_gain;
}

static void *async_reader_thread(void *cx)
{
    NOTICE("BEFORE rtlsdr_read_async\n");
    rtlsdr_read_async(dev, async_reader_cb, NULL, 0, 0);
    NOTICE("AFTER rtlsdr_read_async\n");

    free(cx);

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


#if 0
    // xxx add code for test mode
    // test mode 
    #define TEST_MODE_ON  1
    #define TEST_MODE_OFF 0
    rc = rtlsdr_set_testmode(dev, TEST_MODE_ON);

    rc = rtlsdr_reset_buffer(dev);
    NOTICE("rtlsdr_reset_buffer rc=%d\n", rc);

    pthread_t tid;
    pthread_create(&tid, NULL, reader, NULL);
    sleep(5);
    NOTICE("cancelling\n");
    rc = rtlsdr_cancel_async(dev);
    NOTICE("cancel ret %d\n", rc);
    sleep(5);
#endif
