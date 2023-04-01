#include "common.h"
#include <rtl-sdr.h>

#define SAMPLE_RATE 1000000

#define KHZ 1000
#define MHZ 1000000

char *progname = "sdr";

rtlsdr_dev_t *dev;

unsigned long total = 0;
int min = 128, max = 128;

void cb(unsigned char *buf, uint32_t len, void *ctx)
{
    printf("len = %d - buf = %d %d %d %d - %d %d %d %d\n", 
           len, 
           buf[0], buf[1], buf[2], buf[3],
           buf[len-4], buf[len-3], buf[len-2], buf[len-1]);
    total += len;

    for (int i = 0; i < len; i++) {
        if (buf[i] < min) min = buf[i];
        if (buf[i] > max) max = buf[i];
    }
}

void * reader(void *cx)
{
    unsigned long start = microsec_timer();
    unsigned long duration;

    NOTICE("BEFORE rtlsdr_read_async\n");
    rtlsdr_read_async(dev, cb, NULL, 0, 0);
    NOTICE("AFTER rtlsdr_read_async\n");

    duration = microsec_timer() - start;

    NOTICE("total = %ld\n", total);
    NOTICE("bytes/sec = %ld\n", total * 1000000 / duration);
    NOTICE("min = %d   max = %d\n", min, max);

    return NULL;
}

int main(int argc, char **argv)
{
    int rc, dev_cnt, i;

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

    // open
    rc = rtlsdr_open(&dev, 0);
    if (rc != 0) {
        ERROR("rtlsdr_open rc=%d\n", rc);
        exit(1);
    }
    NOTICE("opened index=0\n");

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
    NOTICE("tuner=%d\n", tuner);   // is RTLSDR_TUNER_R820T

    // get gains
    char str[500], *p=str;
    int num_gains, *gains;
    num_gains = rtlsdr_get_tuner_gains(dev, NULL);
    gains = calloc(num_gains, sizeof(int));
    rtlsdr_get_tuner_gains(dev, gains);
    p += sprintf(p, "num_gains=%d: ", num_gains);
    for (i = 0; i < num_gains; i++) {
        p += sprintf(p, "%d ", gains[i]);
    }
    NOTICE("%s\n", str);
    int max_gain = gains[num_gains-1];
    NOTICE("max_gain = %d\n", max_gain);

    // get gain mode manual, and set max_gain
    #define GAIN_MODE_AUTO   0
    #define GAIN_MODE_MANUAL 1
    int curr_gain;
    rtlsdr_set_tuner_gain_mode(dev, GAIN_MODE_MANUAL);
    rtlsdr_set_tuner_gain(dev, max_gain);
    curr_gain = rtlsdr_get_tuner_gain(dev);
    NOTICE("curr_gain = %d\n", curr_gain);

    // set sample rate
    unsigned int curr_sample_rate;
    rtlsdr_set_sample_rate(dev, SAMPLE_RATE);
    curr_sample_rate = rtlsdr_get_sample_rate(dev);
    NOTICE("curr_sample_rate = %u\n", curr_sample_rate);

    // disable agc mode
    #define AGC_MODE_ENABLE  1
    #define AGC_MODE_DISABLE 0
    rtlsdr_set_agc_mode(dev, AGC_MODE_DISABLE);

#if 0
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

    NOTICE("closing\n");
    rtlsdr_close(dev);
    return 0;
#endif

#if 1
    // direct sampling
    #define DIRECT_SAMPLING_DISABLED          0
    #define DIRECT_SAMPLING_I_ADC_ENABLED     1
    #define DIRECT_SAMPLING_Q_ADC_ENABLED     2

    int direct_sampling_state;
    rc = rtlsdr_set_direct_sampling(dev, DIRECT_SAMPLING_Q_ADC_ENABLED);  // xxx can both be enabled
    if (rc != 0) {
        ERROR("rtlsdr_set_direct_sampling\n");
        exit(1);
    }
    NOTICE("direct sampling enabled\n");

    direct_sampling_state = rtlsdr_get_direct_sampling(dev);
    NOTICE("direct sampling state = %d\n", direct_sampling_state);

    #define WBZ (1030 * KHZ)
    #define OTHER_FREQ (6000 * KHZ)
    unsigned int ctr_freq;
    rtlsdr_set_center_freq(dev, OTHER_FREQ);
    ctr_freq = rtlsdr_get_center_freq(dev);
    NOTICE("ctr_freq %d  %f MHZ\n", ctr_freq, (double)ctr_freq/MHZ);

    rc = rtlsdr_reset_buffer(dev);
    NOTICE("rtlsdr_reset_buffer rc=%d\n", rc);

    pthread_t tid;
    pthread_create(&tid, NULL, reader, NULL);
    sleep(5);
    NOTICE("cancelling\n");
    rc = rtlsdr_cancel_async(dev);
    NOTICE("cancel ret %d\n", rc);
    sleep(5);

    NOTICE("closing\n");
    rtlsdr_close(dev);
    return 0;
#endif


    // NOTES FOLLOW ...

#if 0   // sync
    unsigned char buf[512];  // xxx are char signed
    int n_read = 0;
    rc = rtlsdr_read_sync(dev, buf, sizeof(buf), &n_read);
    if (rc != 0) {
        ERROR("rtlsdr_read_sync rc=%d\n", rc);
        exit(1);
    }
    NOTICE("rtlsdr_read_sync n_read = %d\n", n_read);
    for (i = 0; i < sizeof(buf); i++) {
        NOTICE("%u\n", buf[i]);
    }
#endif

#if 0
RTLSDR_API int rtlsdr_set_center_freq(rtlsdr_dev_t *dev, uint32_t freq);
RTLSDR_API uint32_t rtlsdr_get_center_freq(rtlsdr_dev_t *dev);
RTLSDR_API int rtlsdr_set_freq_correction(rtlsdr_dev_t *dev, int ppm);
RTLSDR_API int rtlsdr_get_freq_correction(rtlsdr_dev_t *dev);

zero is automatic
RTLSDR_API int rtlsdr_set_tuner_bandwidth(rtlsdr_dev_t *dev, uint32_t bw);

RTLSDR_API int rtlsdr_set_tuner_if_gain(rtlsdr_dev_t *dev, int stage, int gain);

 *                  225001 - 300000 Hz
 *                  900001 - 3200000 Hz
 *                  sample loss is to be expected for rates > 2400000
RTLSDR_API int rtlsdr_set_sample_rate(rtlsdr_dev_t *dev, uint32_t rate);

 * Enable or disable the direct sampling mode. When enabled, the IF mode
 * of the RTL2832 is activated, and rtlsdr_set_center_freq() will control
 * the IF-frequency of the DDC, which can be used to tune from 0 to 28.8 MHz
 * (xtal frequency of the RTL2832).
* \param on 0 means disabled, 1 I-ADC input enabled, 2 Q-ADC input enabled

RTLSDR_API int rtlsdr_set_direct_sampling(rtlsdr_dev_t *dev, int on);

RTLSDR_API int rtlsdr_set_offset_tuning(rtlsdr_dev_t *dev, int on);
#endif


#if 0
RTLSDR_API int rtlsdr_set_center_freq(rtlsdr_dev_t *dev, uint32_t freq);
RTLSDR_API uint32_t rtlsdr_get_center_freq(rtlsdr_dev_t *dev);
RTLSDR_API int rtlsdr_set_freq_correction(rtlsdr_dev_t *dev, int ppm);
RLSDR_API int rtlsdr_get_freq_correction(rtlsdr_dev_t *dev);
#endif

/**
    // Tuner (ie. E4K/R820T/etc) auto gain
    //rtlsdr_set_tuner_gain_mode(device, 1);
    //rtlsdr_set_tuner_gain(device, 420);
    // RTL2832 auto gain off

    rtlsdr_reset_buffer(device);
    rtlsdr_set_center_freq(device, freq);
    rtlsdr_set_sample_rate(device, sample_rate);
#if 1
    rtlsdr_set_tuner_gain_mode(device, 1);
    gain_default();
#else
    rtlsdr_set_tuner_gain_mode(device, 0);
    rtlsdr_set_agc_mode(device, 1);
#endif

    //rtlsdr_set_agc_mode(device, 0);
    //rtlsdr_set_freq_correction(device, 0);

    fprintf(stderr, "Frequency set to %d\n", rtlsdr_get_center_freq(device));

    // Flush the buffer
    rtlsdr_reset_buffer(device);
**/
    NOTICE("closing\n");
    rtlsdr_close(dev);
    return 0;
}
