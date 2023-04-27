#include "common.h"

//
// defines
//

// xxx comment on why these are lower
#define SAMPLE_RATE  2400000   // 2.4 MS/sec
#define F_SYNTH       500000   // 0.5 MHz  500 KHz
#define F_LPF         100000   // 0.1 MHz  100 KHz

#define MAX_IQ       (SAMPLE_RATE / 10)   // 0.1 sec range

#define DELTA_T (1. / SAMPLE_RATE)

#define W_SYNTH (TWO_PI * F_SYNTH)

//
// variables
//

char *progname = "gen";
bool  debug;

//
// prototypes
//

void init_antenna(void);
double get_antenna(double t);

void lpf(complex *data, int max, int freq);//xxx


// -----------------  MAIN  --------------------------------------------

int main(int argc, char **argv)
{
    double t, tmax, antenna, synth0, synth90;
    double    ig_i, ig_q;
    double    max_iq_i=0, max_iq_q=0;
    int       i=0, j;

    static complex       iq[MAX_IQ];
    static unsigned char usb[2*MAX_IQ];

    tmax = 10;  // xxx arg, default is 60

    init_antenna();
    
    for (t = 0; t < tmax; t += DELTA_T) {
        antenna = get_antenna(t);

        synth0  = sin((W_SYNTH) * t);
        synth90 = sin((W_SYNTH) * t + M_PI_2);

        iq[i++] = antenna * (synth0 + I * synth90);

        if (i == MAX_IQ) {
            NOTICE("t %f\n", t);

            //lpf(iq, MAX_IQ, F_LPF);

            for (j = 0; j < MAX_IQ; j++) {
                ig_i = creal(iq[j]);
                ig_q = cimag(iq[j]);

                if (fabs(ig_i) > max_iq_i) max_iq_i = fabs(ig_i);
                if (fabs(ig_q) > max_iq_q) max_iq_q = fabs(ig_q);

                #define MAX_IQ_VALUE 10.0
                #define CONVERT(x) nearbyint((128 / MAX_IQ_VALUE) * ((x) + MAX_IQ_VALUE))  // xxx check this

                usb[2*j+0] = CONVERT(ig_i);
                usb[2*j+1] = CONVERT(ig_q);
                //NOTICE("%u %u\n", usb[2*j+0], usb[2*j+1]);
            }

            fwrite(usb, sizeof(unsigned char), MAX_IQ*2, stdout);

            i = 0;
        }
    }

    NOTICE("max i=%f  q=%f  MAX_IQ_VALUE=%f\n", max_iq_i, max_iq_q, MAX_IQ_VALUE);
    if (max_iq_i > MAX_IQ_VALUE || max_iq_q > MAX_IQ_VALUE) {
        ERROR("exceeds MAX_IQ_VALUE %f\n", MAX_IQ_VALUE);
    }

    return 0;
}

// -----------------  GET ANTENNA SIGNAL  ------------------------------

#define MAX_AUDIO 5
#define F_DELTA   20000

double get_audio(int id, double t);
void init_audio_wav_file(int id, char *filename);
void init_audio_sine_wave(int id, int f);
void init_audio_white_noise(int id);

double get_antenna(double t)
{
    double f, y;
    int i;

    y = 0;
    f = F_SYNTH - F_DELTA * (MAX_AUDIO / 2);

    for (i = 0; i < MAX_AUDIO; i++) {
        // note: get_audio returns value in range -1 to 1
        y += (1 + get_audio(i,t)) * sin(TWO_PI * f * t);
        f += F_DELTA;
    }

    return y;
}

void init_antenna(void)
{
    init_audio_wav_file(0, "proud_mary.wav");
    init_audio_wav_file(1, "one_bourbon_one_scotch_one_beer.wav");
    init_audio_wav_file(2, "super_critical.wav");
    init_audio_sine_wave(3, 300);
    init_audio_white_noise(4);
}

// - - - - - - - - - - - - - - - - - - - - 

struct audio_s {
    int n;
    int sample_rate;
    double *data;
} audio[MAX_AUDIO];

double get_audio(int id, double t)
{
    int idx;
    struct audio_s *a = &audio[id];

    idx = (unsigned long)nearbyint(t * a->sample_rate) % a->n;
    return a->data[idx];
}

void init_audio_wav_file(int id, char *filename)
{
    int    ret, num_chan, num_items, sample_rate;
    struct audio_s *a = &audio[id];
    double *data;

    ret = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0) {
        ERROR("read_wav_file %s, %s\n", filename, strerror(errno));
        exit(1);
    }
    NOTICE("num_chan=%d num_items=%d sample_rate=%d\n", num_chan, num_items, sample_rate);

    if (num_chan != 1) {
        ERROR("num_chan must be 1\n");
        exit(1);
    }

    a->n         = num_items;
    a->sample_rate = sample_rate;
    a->data        = data;

    // xxx lpf_real(a->data, a->n, sample_rate, 3000, filename);

    //lpf
    //normalize
}

void init_audio_sine_wave(int id, int f)
{
    struct audio_s *a = &audio[id];
    double          w = TWO_PI * f;
    double          t, dt;
    int             i;

    a->n = 24000;
    a->sample_rate = 24000;
    a->data = (double*)calloc(a->n, sizeof(double));

    t = 0;
    dt = 1. / a->sample_rate;

    for (i = 0; i < a->n; i++) {
        a->data[i] = sin(w * t);
        t += dt;
    }
}

void init_audio_white_noise(int id)
{
    struct audio_s *a = &audio[id];

    a->n = 24000;
    a->sample_rate = 24000;
    a->data = (double*)calloc(a->n, sizeof(double));

    for (int i = 0; i < a->n; i++) {
        a->data[i] =  2 * (((double)random() / RAND_MAX) - 0.5);
    }

    // lpf
    // normalize
}

