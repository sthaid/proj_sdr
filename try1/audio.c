#include "common.h"

// xxx volume control

#define MAX_AO_BUFF 2000

static double        ao_buff[MAX_AO_BUFF];
static unsigned long ao_head;
static unsigned long ao_tail;

static void *pa_play_thread(void*cx);
static int pa_play_cb(void *data, void *cx_arg);
static void print_max_audio_values(double ao);

// -----------------  AUDIO INIT  -----------------------------------

void audio_init(void)
{
    pthread_t tid;

    // xxx minimize prints
    // xxx program option to display devices
    pa_init();
    pthread_create(&tid, NULL, pa_play_thread, NULL);
}

// -----------------  AUDIO OUTPUT  ---------------------------------

void audio_out(double ao)
{
    #define MAX_MA 10000

    static void *ma_cx;

    // wait for room in audio output ring buffer
    while (ao_tail - ao_head == MAX_AO_BUFF) {
        usleep(1000);
    }

    // center the audio output value at average value of 0
    ao -= moving_avg(ao, MAX_MA, &ma_cx);

    // print max audio value
    print_max_audio_values(ao);

    // add audio out data vale to the tail of the audio out buffer    
    ao_buff[ao_tail%MAX_AO_BUFF] = ao;
    ao_tail++;
}

static void *pa_play_thread(void*cx)
{
    int ret;
    int num_chan = 1;
    int sample_rate = AUDIO_SAMPLE_RATE;

    NOTICE("pa_play_thread starting\n");

    pthread_setname_np(pthread_self(), "sdr_pa_play");

    ret = pa_play2(DEFAULT_OUTPUT_DEVICE, num_chan, sample_rate, PA_FLOAT32, pa_play_cb, NULL);
    if (ret != 0) {
        FATAL("pa_play2\n");
    }

    NOTICE("pa_play_thread terminating\n");

    return NULL;
}

static int pa_play_cb(void *data, void *cx_arg)
{
    while (ao_head == ao_tail) {
        if (program_terminating) {
            return -1;
        }
        usleep(1000);
    }

    *(float*)data = ao_buff[ao_head%MAX_AO_BUFF];
    ao_head++;
    return 0;
}

static void print_max_audio_values(double ao)
{
    static double max_ao, max_ao_abs;
    static int cnt;

    if (fabs(ao) > max_ao_abs) {
        max_ao_abs = fabs(ao);
        max_ao = ao;
    }

    if (++cnt > 5*AUDIO_SAMPLE_RATE) {
        NOTICE("LARGEST AUDIO VAL = %f\n", max_ao);
        max_ao_abs = 0;
        max_ao = 0;
        cnt = 0;
    }
}
