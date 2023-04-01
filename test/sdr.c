#include "common.h"
#include <rtl-sdr.h>

#define SAMPLE_RATE 1000000

char *progname = "sdr";

rtlsdr_dev_t *dev;

unsigned long total = 0;

void cb(unsigned char *buf, uint32_t len, void *ctx)
{
    printf("len = %d - buf = %d %d %d %d - %d %d %d %d\n", 
           len, 
           buf[0], buf[1], buf[2], buf[3],
           buf[len-4], buf[len-3], buf[len-2], buf[len-1]);
    total += len;
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

    return NULL;
}

int main(int argc, char **argv)
{
    int rc, dev_cnt, i;

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

    rc = rtlsdr_open(&dev, 0);
    if (rc != 0) {
        ERROR("rtlsdr_open rc=%d\n", rc);
        exit(1);
    }
    NOTICE("opened index=0\n");

    unsigned int rtl_freq, tuner_freq;
    #define MHZ 1000000
    rc = rtlsdr_get_xtal_freq(dev, &rtl_freq, &tuner_freq);
    NOTICE("rtl_freq=%0.2f  tuner_freq=%0.2f\n", (double)rtl_freq/MHZ, (double)tuner_freq/MHZ);

    char manufact[256], product[256], serial[256];
    rc = rtlsdr_get_usb_strings(dev, manufact, product, serial);
    NOTICE("manufact='%s'  product='%s'  serial='%s'\n",
           manufact, product, serial);

    enum rtlsdr_tuner tuner;
    tuner = rtlsdr_get_tuner_type(dev);
    NOTICE("tuner=%d\n", tuner);   // is RTLSDR_TUNER_R820T

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


    #define GAIN_MODE_AUTO   0
    #define GAIN_MODE_MANUAL 1
    int curr_gain;
    rtlsdr_set_tuner_gain_mode(dev, GAIN_MODE_MANUAL);
    rtlsdr_set_tuner_gain(dev, max_gain);
    curr_gain = rtlsdr_get_tuner_gain(dev);
    NOTICE("curr_gain = %d\n", curr_gain);

    unsigned int curr_sample_rate;
    rtlsdr_set_sample_rate(dev, SAMPLE_RATE);
    curr_sample_rate = rtlsdr_get_sample_rate(dev);
    NOTICE("curr_sample_rate = %u\n", curr_sample_rate);

    // xxx tru rtlsdr_set_testmode

    #define AGC_MODE_ENABLE  1
    #define AGC_MODE_DISABLE 0
    rtlsdr_set_agc_mode(dev, AGC_MODE_DISABLE);


#if 0
    #define DIRECT_SAMPLING_DISABLED          0
    #define DIRECT_SAMPLING_I_ADC_ENABLED     1
    #define DIRECT_SAMPLING_Q_ADC_ENABLED     2
    int direct_sampling_state;
    rc = rtlsdr_set_direct_sampling(dev, DIRECT_SAMPLING_I_ADC_ENABLED);  // xxx can both be enabled
    if (rc != 0) {
        ERROR("rtlsdr_set_direct_sampling\n");
        exit(1);
    }
    NOTICE("direct sampling enabled\n");
    direct_sampling_state = rtlsdr_get_direct_sampling(dev);
    NOTICE("direct sampling state = %d\n", direct_sampling_state);
#endif


    // streaming functions
    // - zero point of data is  128


    // test mode read
    #define TEST_MODE_ON  1
    #define TEST_MODE_OFF 0
    rc = rtlsdr_set_testmode(dev, TEST_MODE_ON);

    rc = rtlsdr_reset_buffer(dev);
    NOTICE("rtlsdr_reset_buffer rc=%d\n", rc);

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
#else
    pthread_t tid;
    pthread_create(&tid, NULL, reader, NULL);

    sleep(5);
    NOTICE("cancelling\n");
    rc = rtlsdr_cancel_async(dev);
    NOTICE("cancel ret %d\n", rc);
    sleep(5);
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
