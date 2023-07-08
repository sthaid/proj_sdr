#include "common.h"
// xxx volume control

#define USE_APLAY

#ifdef USE_APLAY

// ==================================================
// Using Aplay 
// ==================================================

static FILE *fp;

void audio_init(void)
{
    char cmd[100];

    sprintf(cmd, "aplay -t raw -c 1 -f FLOAT_LE -r %d", AUDIO_SAMPLE_RATE);

    fp = popen(cmd, "w");
    if (fp == NULL) {
        FATAL("failed to run aplay\n");
    }
}

void audio_out(double ao)
{
    #define MAX_MA  10000
    #define MAX_OUT 1000

    static double max_ao = -999, min_ao = 999;
    static int    cnt, max_out;
    static void  *ma_cx;
    static float  out[MAX_OUT];

    // center ao at zero
    ao -= moving_avg(ao, MAX_MA, &ma_cx);

    // write audio to stdout, once every MAX_OUT values
    out[max_out++] = ao;
    if (max_out == MAX_OUT) {
        fwrite(out, sizeof(float), MAX_OUT, fp);
        max_out = 0;
    }

    // maintain and print stats
    if (ao > max_ao) {
        max_ao = ao;
    }
    if (ao < min_ao) {
        min_ao = ao;
    }
    if (++cnt == 5 * AUDIO_SAMPLE_RATE) {
        NOTICE("min/max_ao = %f %f\n", min_ao, max_ao);
        max_ao = -999;
        min_ao = 999;
        cnt    = 0;
    }
}

#else

// ==================================================
// Using Portaudio 
// ==================================================

#define MAX_AO_BUFF 2000

static double        ao_buff[MAX_AO_BUFF];
static unsigned long ao_head;
static unsigned long ao_tail;

static bool program_terminating;

static void exit_hndlr(void);
static void *pa_play_thread(void*cx);
static int pa_play_cb(void *data, void *cx_arg);

// -----------------  AUDIO INIT  -----------------------------------

void audio_init(void)
{
    pthread_t tid;

    // xxx minimize prints
    // xxx program option to display devices
    pa_init();
    pthread_create(&tid, NULL, pa_play_thread, NULL);

    atexit(exit_hndlr);
}

static void exit_hndlr(void)
{
    NOTICE("%s exit_hndlr\n", __FILE__);

    program_terminating = true;
}

// -----------------  AUDIO OUTPUT  ---------------------------------

void audio_out(double ao)
{
    #define MAX_MA 10000

    static void  *ma_cx;
    static int    rb_full_cnt, rb_empty_cnt, cnt;
    static double max_ao=-999, min_ao=999;

    bool rb_full  = ((ao_tail - ao_head) == MAX_AO_BUFF);
    bool rb_empty = ((ao_tail - ao_head) == 0);

    // center the audio output value at average value of 0
    ao -= moving_avg(ao, MAX_MA, &ma_cx);

    // add audio out data vale to the tail of the audio out buffer    
    if (!rb_full) {
        ao_buff[ao_tail%MAX_AO_BUFF] = ao;
        ao_tail++;
    }

    // maintain and print stats
    if (rb_full) {
        rb_full_cnt++;
    }
    if (rb_empty) {
        rb_empty_cnt++;
    }
    if (ao > max_ao) {
        max_ao = ao;
    }
    if (ao < min_ao) {
        min_ao = ao;
    }
    if (++cnt == 5 * AUDIO_SAMPLE_RATE) {
        NOTICE("min/max_ao = %f %f  rb_full_cnt = %d  rb_empty_cnt = %d\n", 
               min_ao, max_ao, rb_full_cnt, rb_empty_cnt);
        rb_full_cnt  = 0;
        rb_empty_cnt = 0;
        max_ao       = -999;
        min_ao       = 999;
        cnt          = 0;
    }
}

static void *pa_play_thread(void*cx)
{
    int ret;
    int num_chan = 1;

    NOTICE("pa_play_thread starting\n");

    pthread_setname_np(pthread_self(), "sdr_pa_play");

    ret = pa_play2(DEFAULT_OUTPUT_DEVICE, num_chan, AUDIO_SAMPLE_RATE, PA_FLOAT32, pa_play_cb, NULL);
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
#endif
