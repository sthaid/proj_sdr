#include "common.h"

#include <sndfile.h>

// reference:
//   apt install libsndfile1-dev
//   file:///usr/share/doc/libsndfile1-dev/html/api.html
//   https://github.com/libsndfile/libsndfile

// xxx review
// sine [hz]
// white
// wav <xxx.wav>

char *progname = "src";

static int sine(char *freq_str);
static int white(char *arg);
static int wav(char *filename);

// xxx print the avg or running amplitude

int main(int argc, char **argv)
{
    char *arg1, *arg2;
    int rc;

    if (argc < 2) {
        ERROR("arg expected\n");
        return 1;
    }

    arg1 = argv[1];
    arg2 = argc > 2 ? argv[2] : "";

    if (strcmp(arg1, "sine") == 0) {
        rc = sine(arg2);
    } else if (strcmp(arg1, "white") == 0) {
        rc = white(arg2);
    } else if (strcmp(arg1, "wav") == 0) {
        rc = wav(arg2);
    } else {
        ERROR("invalid arg '%s'\n", arg1);
        rc = -1;
        return 1;
    }

    return rc == 0 ? 0 : 1;
}

// -----------------  SINE  ----------------------------

static int sine(char *freq_str)
{
    float  data[SAMPLE_RATE];
    int    i;
    double amp = 1;
    double freq = 1000;

    if (freq_str[0] != '\0') {
        if (sscanf(freq_str, "%lf", &freq) != 1) {
            ERROR("freq '%s' not a number\n", freq_str);
            return -1;
        }
        if (freq < 100 || freq > 10000) {
            ERROR("freq '%s' not in range 100 to 10000 hz\n", freq_str);
            return -1;
        }
    }

    for (i = 0; i < SAMPLE_RATE; i++) {
        data[i] = amp * sin(freq * TWO_PI * ((double)i/SAMPLE_RATE));
    }

    while (true) {
        fwrite(data, sizeof(float), SAMPLE_RATE, stdout);
    }

    return 0;
}

// -----------------  WHITE ----------------------------

static int white(char *arg)
{
    float  data[SAMPLE_RATE];
    int    i;
    double amp = 1;

    #define RAND_RANGE_PLUS_MINUS_ONE ((double)(random() - 0x40000000) / 0x40000000)

    for (i = 0; i < SAMPLE_RATE; i++) {
        data[i] = RAND_RANGE_PLUS_MINUS_ONE * amp;
    }

    while (true) {
        fwrite(data, sizeof(float), SAMPLE_RATE, stdout);
    }

    return 0;

}

// -----------------  WAV  -----------------------------
        
static int read_wav_file(char *filename, float **data, int *num_chan, int *num_items, int *sample_rate);

static int wav(char *filename)
{
    int    rc, num_chan, num_items, sample_rate;
    float *data;

    #define MIN_SAMPLE_RATE (0.9 * SAMPLE_RATE)
    #define MAX_SAMPLE_RATE (1.1 * SAMPLE_RATE)

    #define DEFAULT_FILENAME "super_critical.wav"

    if (filename[0] == '\0') {
        filename = DEFAULT_FILENAME;
    }

    rc = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (rc != 0) {
        ERROR("read_wav_file %s, %s\n", filename, strerror(errno));
        exit(1);
    }

    NOTICE("num_chan=%d num_items=%d sample_rate=%d\n", num_chan, num_items, sample_rate);

    if (num_chan != 1) {
        ERROR("num_chan must be 1\n", num_chan);
        return -1;
    }

    if (sample_rate < MIN_SAMPLE_RATE || sample_rate > MAX_SAMPLE_RATE) {
        ERROR("sample_rate=%d, out of range %d - %d\n",
              sample_rate, MIN_SAMPLE_RATE, MAX_SAMPLE_RATE);
        return -1;
    }

    while (true) {
        fwrite(data, sizeof(float), num_items, stdout);
    }

    return 0;
}

// caller must free returned data when done
int read_wav_file(char *filename, float **data, int *num_chan, int *num_items, int *sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt, items;
    float   *d;

    // preset return values
    *data = NULL;
    *num_chan = 0;
    *num_items = 0; 
    *sample_rate = 0;

    // open wav file and get info
    memset(&sfinfo, 0, sizeof (sfinfo));
    file = sf_open(filename, SFM_READ, &sfinfo);
    if (file == NULL) {
        //ERROR("sf_open '%s'\n", filename);
        return -1;
    }

    // allocate memory for the data
    items = sfinfo.frames * sfinfo.channels;
    d = malloc(items*sizeof(float));

    // read the wav file data 
    cnt = sf_read_float(file, d, items);
    if (cnt != items) {
        //ERROR("sf_read_float, cnt=%d items=%d\n", cnt, items);
        free(d);
        sf_close(file);
        return -1;
    }

    // close file
    sf_close(file);

    // return values
    *data        = d;
    *num_chan    = sfinfo.channels;
    *num_items   = items;
    *sample_rate = sfinfo.samplerate;
    return 0;
}
