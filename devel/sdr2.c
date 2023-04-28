#include "common.h"
#include <rtl-sdr.h>

// xxx check ret codes
// xxx may want this when shuttind down
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

#define KHZ 1000
#define MHZ 1000000

//
// typedef
//

typedef struct {
    rtlsdr_dev_t *dev;
    void (*cb)(unsigned char *iq, size_t len);
} cx_t;

//
// prototypes
//

static void print_info(rtlsdr_dev_t *dev);
static void get_gains(rtlsdr_dev_t *dev, int *num_gains_arg, int *gains_arg);
static int get_max_gain(rtlsdr_dev_t *dev);
static void *async_reader_thread(void *cx);
static void async_reader_cb(unsigned char *buf, unsigned int len, void *cx_arg);

// -----------------  API  ---------------------------------------------

// xxx call this at startup
void sdr_list_devices(void)
{
    // list devices
    dev_cnt = rtlsdr_get_device_count();
    NOTICE("dev_cnt = %d\n", dev_cnt);
    if (dev_cnt == 0) {
        ERROR("dev_cnt = 0\n");
        exit(1);
    }

    for (i = 0; i < dev_cnt; i++) {
        char manufact[256], product[256], serial[256];

        rc = rtlsdr_get_device_usb_strings(0, manufact, product, serial);
        if (rc != 0) {
            ERROR("rtlsdr_get_device_usb_strings(%d) rc=%d\n", i, rc);
            exit(1);
        }
        NOTICE("name='%s'  manufact='%s'  product='%s'  serial='%s'\n",
               rtlsdr_get_device_name(i), manufact, product, serial);
    }
}

// xxx pass in idx, sample_rate, ...
void sdr_init(double f, void(*cb)(unsigned char *iq, size_t len))
{
    int rc;
    rtlsdr_dev_t *dev;

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

    // enable direct sampling, xxx explain QA_ADC
    int direct_sampling;
    rc = rtlsdr_set_direct_sampling(dev, DIRECT_SAMPLING_Q_ADC_ENABLED);
    if (rc != 0) {
        FATAL("rtlsdr_set_direct_sampling\n");
    }
    direct_sampling = rtlsdr_get_direct_sampling(dev);
    NOTICE("curr direct sampling = %s\n", DIRECT_SAMPLE_STR(direct_sampling));

    // set center frequency
    unsigned int ctr_freq;
    rtlsdr_set_center_freq(dev, f);
    ctr_freq = rtlsdr_get_center_freq(dev);
    NOTICE("curr ctr_freq %d  %f MHZ\n", ctr_freq, (double)ctr_freq/MHZ);

    // start async reader
    pthread_t tid;
    cx_t *cx;
    rc = rtlsdr_reset_buffer(dev);
    if (rc != 0) {
        FATAL("rtlsdr_reset_buffer\n");
    }
    cx = malloc(sizoef(cx_t));
    cx->dev = dev;
    cx->cb = cb;
    pthread_create(&tid, NULL, async_reader_thread, cx);
}

void sdr_set_freq(double f)
{
    rtlsdr_set_center_freq(dev, f);
}

// -----------------  LOCAL  -------------------------------------------

static void print_info(rtlsdr_dev_t *dev)
{
    // get xtal freq
    unsigned int rtl_freq, tuner_freq;
    rc = rtlsdr_get_xtal_freq(dev, &rtl_freq, &tuner_freq);
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
    get_gains(dev, &num_gains, &gains);
    p += sprintf(p, "num_gains=%d: ", num_gains);
    for (i = 0; i < num_gains; i++) {
        p += sprintf(p, "%d ", gains[i]);
    }
    NOTICE("%s\n", str);
    NOTICE("max_gain = %d\n", gains[num_gains-1]);
    free(gains);
}

static void get_gains(rtlsdr_dev_t *dev, int *num_gains_arg, int *gains_arg)
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
    int num_gains, *gains;

    get_gains(dev, &num_gains, &gains);
    max_gain = gains[num_gains-1];
    free(gains);
    return max_gain;
}

static void *async_reader_thread(void *cx)
{
    rtl_sdr_t *dev = cx;

    NOTICE("BEFORE rtlsdr_read_async\n");
    rtlsdr_read_async(cx->dev, async_reader_cb, cx, 0, 0);
    NOTICE("AFTER rtlsdr_read_async\n");

    free(cx);

    return NULL;
}

static void async_reader_cb(unsigned char *buf, unsigned int len, void *cx_arg)
{
    cx_t * cx = cx_arg;

    cx->cb(buf, len);
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
